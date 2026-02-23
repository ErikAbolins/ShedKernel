#include "kbd_driver.h"
#include "mm.h"
#include "time.h"

#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE * LINES


#define IDT_SIZE 256
#define GDT_SIZE 8
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08
#define GDTBASE 0x00000800


#define PIT_FREQ 1193180
#define HZ 100

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;


extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);
extern void kbd_handler(void);
extern void timer_handler(void);
volatile uint32_t jiffies = 0;
unsigned int current_loc = 0;
char *vidptr = (char*)0xb8000;

/* TSS structure */
struct tss {
    u32 prev_tss;
    u32 esp0;
    u32 ss0;
    u32 esp1;
    u32 ss1;
    u32 esp2;
    u32 ss2;
    u32 cr3;
    u32 eip;
    u32 eflags;
    u32 eax, ecx, edx, ebx;
    u32 esp, ebp, esi, edi;
    u32 es, cs, ss, ds, fs, gs;
    u32 ldt;
    u16 trap;
    u16 io_map;
} __attribute__((packed));

struct tss default_tss;

void *memcpy(void *dest, const void *src, unsigned int n)
{
    char *d = dest;
    const char *s = src;
    while (n--)
        *d++ = *s++;
    return dest;
}

/* IDT */
struct IDT_entry {
    u16 offset_lowerbits;
    u16 selector;
    u8  zero;
    u8  type_attr;
    u16 offset_higherbits;
};

struct IDT_entry IDT[IDT_SIZE];

/* GDT */
struct GDT_entry {
    u16 lim0_15;
    u16 base0_15;
    u8  base16_23;
    u8  acces;
    u8  lim16_19 : 4;
    u8  other    : 4;
    u8  base24_31;
} __attribute__((packed));

struct GDT_entry kgdt[GDT_SIZE];

struct {
    u16 limite;
    u32 base;
} __attribute__((packed)) kgdtr;

void init_gdt_descriptor(u32 base, u32 limite, u8 acces, u8 other, struct GDT_entry *entry)
{
    entry->lim0_15   = (limite & 0xffff);
    entry->base0_15  = (base & 0xffff);
    entry->base16_23 = (base & 0xff0000) >> 16;
    entry->acces     = acces;
    entry->lim16_19  = (limite & 0xf0000) >> 16;
    entry->other     = (other & 0xf);
    entry->base24_31 = (base & 0xff000000) >> 24;
}

void gdt_init(void)
{
    default_tss.io_map = 0x00;
    default_tss.esp0   = 0x1FFF0;
    default_tss.ss0    = 0x18;

    init_gdt_descriptor(0x0, 0x0,      0x00, 0x00, &kgdt[0]); /* null */
    init_gdt_descriptor(0x0, 0xFFFFF,  0x9B, 0x0D, &kgdt[1]); /* kernel code */
    init_gdt_descriptor(0x0, 0xFFFFF,  0x93, 0x0D, &kgdt[2]); /* kernel data */
    init_gdt_descriptor(0x0, 0x0,      0x97, 0x0D, &kgdt[3]); /* kernel stack */
    init_gdt_descriptor(0x0, 0xFFFFF,  0xFF, 0x0D, &kgdt[4]); /* user code */
    init_gdt_descriptor(0x0, 0xFFFFF,  0xF3, 0x0D, &kgdt[5]); /* user data */
    init_gdt_descriptor(0x0, 0x0,      0xF7, 0x0D, &kgdt[6]); /* user stack */
    init_gdt_descriptor((u32)&default_tss, 0x67, 0xE9, 0x00, &kgdt[7]); /* TSS */

    kgdtr.limite = sizeof(kgdt) - 1;
    kgdtr.base   = GDTBASE;

    memcpy((void*)GDTBASE, (void*)kgdt, sizeof(kgdt));

    asm volatile("lgdt %0" : : "m"(kgdtr));

    asm volatile( //declare the gdt table here because i'm too lazy to write in the bootloader
        "movw $0x10, %ax\n"
        "movw %ax, %ds\n"
        "movw %ax, %es\n"
        "movw %ax, %fs\n"
        "movw %ax, %gs\n"
        "ljmp $0x08, $1f\n"
        "1:\n"
    );
}

void idt_init(void)
{
    unsigned long keyboard_address;
    unsigned long idt_address;
    unsigned long idt_ptr[2];

    keyboard_address = (unsigned long)kbd_handler;
    IDT[0x21].offset_lowerbits  = keyboard_address & 0xffff;
    IDT[0x21].selector          = KERNEL_CODE_SEGMENT_OFFSET;
    IDT[0x21].zero              = 0;
    IDT[0x21].type_attr         = INTERRUPT_GATE;
    IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;

    unsigned long timer_address = (unsigned long)timer_handler;
    IDT[0x20].offset_lowerbits  = timer_address & 0xffff;
    IDT[0x20].selector          = KERNEL_CODE_SEGMENT_OFFSET;
    IDT[0x20].zero              = 0;
    IDT[0x20].type_attr         = INTERRUPT_GATE;
    IDT[0x20].offset_higherbits = (timer_address >> 16);


    /* ICW1 - begin initialization */
    write_port(0x20, 0x11);
    write_port(0xA0, 0x11);

    /* ICW2 - vector offsets */
    write_port(0x21, 0x20);
    write_port(0xA1, 0x28);

    /* ICW3 - cascading */
    write_port(0x21, 0x04);
    write_port(0xA1, 0x02);

    /* ICW4 - environment */
    write_port(0x21, 0x01);
    write_port(0xA1, 0x01);

    /* mask all interrupts */
    write_port(0x21, 0xFC); //keyboard and timer
    write_port(0xA1, 0xFE);

    idt_address = (unsigned long)IDT;
    idt_ptr[0] = (sizeof(struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
    idt_ptr[1] = idt_address >> 16;

    load_idt(idt_ptr);
}

void pit_init() {
    unsigned short divisor = PIT_FREQ / HZ;

    write_port(0x43, 0x36);
    write_port(0x40, divisor & 0xFF);
    write_port(0x40, divisor >> 8);
}

void timer_callback() {
    jiffies++;
    write_port(0x20, 0x20);
}


uint32_t uptime() {
    return jiffies / HZ;
}


void kprint(const char *str)
{
    unsigned int i = 0;
    while (str[i] != '\0') {
        vidptr[current_loc++] = str[i++];
        vidptr[current_loc++] = 0x07;
    }
}

void kprint_newline(void)
{
    unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
    current_loc = current_loc + (line_size - current_loc % line_size);
}

void clear_screen(void)
{
    unsigned int i = 0;
    while (i < SCREENSIZE) {
        vidptr[i++] = ' ';
        vidptr[i++] = 0x07;
    }
}

//malloc test
void kprint_hex(uint32_t val) {
    char buf[11] = "0x00000000";
    char hex[] = "0123456789ABCDEF";
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    kprint(buf);
    kprint_newline();
}


void kernel_main(void)
{
    const char *str = "darcy poo head";
    clear_screen();
    init_dynamic_mem();

    // basic alloc
    uint32_t *a = malloc(sizeof(uint32_t));
    uint32_t *b = malloc(sizeof(uint32_t));
    uint32_t *c = malloc(sizeof(uint32_t));

    *a = 111;
    *b = 222;
    *c = 333;

    // print addresses
    kprint("a: "); kprint_hex((uint32_t)a);
    kprint("b: "); kprint_hex((uint32_t)b);
    kprint("c: "); kprint_hex((uint32_t)c);

    // free middle block and reallocate
    mem_free(b);
    uint32_t *d = malloc(sizeof(uint32_t));
    *d = 444;
    kprint("d: "); kprint_hex((uint32_t)d);  // should reuse b's address

    // free all and reallocate big block
    mem_free(a);
    mem_free(c);
    mem_free(d);
    uint32_t *big = malloc(1024);
    kprint("big: "); kprint_hex((uint32_t)big);  // please work

    kprint(str);
    kprint_newline();
    struct tm t;
    t.tm_sec = 0;
    t.tm_min = 0;
    t.tm_hour = 0;
    t.tm_mday = 1;
    t.tm_mon = 0;
    t.tm_year = 70;
    long result = kernel_mktime(&t);
    kprint("epoch: ");
    kprint_hex((uint32_t)result);
    kprint_newline();


    gdt_init();
    idt_init();
    pit_init();
    write_port(0x21, read_port(0x21) & ~0x01);
    asm volatile("sti");
    kbd_init();
    kbd_enable();


    while (1);
}