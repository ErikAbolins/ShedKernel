/*
 * kernel.c - Kernel entry point and core subsystems
 *
 * Covers: GDT, IDT, paging, PIT, TSS, syscall dispatch,
 *         userspace jumps, and kernel_main.
 */

#include "kbd_driver.h"
#include "mm.h"
#include "time.h"
#include "kprintf.h"
#include "Shell.h"
#include "easyfs.h"
#include "ed.h"


/* =========================================================
 * Constants
 * ========================================================= */

#define IDT_SIZE                256
#define GDT_SIZE                8
#define INTERRUPT_GATE          0x8e
#define KERNEL_CODE_SEGMENT_OFFSET  0x08
#define GDTBASE                 0x00000800

#define PIT_FREQ                1193180
#define HZ                      100

/* Paging flags */
#define PAGE_PRESENT    (1 << 0)
#define PAGE_WRITE      (1 << 1)
#define PAGE_USER       (1 << 2)


/* =========================================================
 * Type aliases
 * ========================================================= */

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;


/* =========================================================
 * Externals
 * ========================================================= */

extern char  read_port(unsigned short port);
extern void  write_port(unsigned short port, unsigned char data);
extern void  load_idt(unsigned long *idt_ptr);
extern void  kbd_handler(void);
extern void  timer_handler(void);
extern u32   kernel_end;

extern char _binary_user_flat_start[];
extern char _binary_user_flat_end[];
extern char _binary_user_shell_flat_start[];
extern char _binary_user_shell_flat_end[];
extern char _binary_user_editor_flat_start[];
extern char _binary_user_editor_flat_end[];

extern char        *vidptr;
extern unsigned int current_loc;


/* =========================================================
 * Globals
 * ========================================================= */

u32 kernel_esp_save      = 0;
u32 exit_requested       = 0;
volatile uint32_t jiffies = 0;

/* Physical page tables - placed at fixed addresses */
u32 *page_directory     = (u32*)0x9C000;
u32 *page_table_kernel  = (u32*)0x9D000;
u32 *page_table_kernel2 = (u32*)0x9E000;


/* =========================================================
 * Structs
 * ========================================================= */

typedef struct {
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
} registers_t;

/* Task State Segment */
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

/* Interrupt Descriptor Table entry */
struct IDT_entry {
    u16 offset_lowerbits;
    u16 selector;
    u8  zero;
    u8  type_attr;
    u16 offset_higherbits;
};

struct IDT_entry IDT[IDT_SIZE];

/* Global Descriptor Table entry */
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


/* =========================================================
 * Utility
 * ========================================================= */

void *memcpy(void *dest, const void *src, unsigned int n)
{
    char *d = dest;
    const char *s = src;
    while (n--)
        *d++ = *s++;
    return dest;
}


/* =========================================================
 * Paging
 * ========================================================= */

void init_paging(void)
{
    for (int i = 0; i < 1024; i++) {
        page_directory[i]    = 0;
        page_table_kernel[i] = (i * 0x1000) | PAGE_PRESENT | PAGE_WRITE;
        page_table_kernel2[i] = (0x400000 + i * 0x1000) | PAGE_PRESENT | PAGE_WRITE;
    }

    page_directory[0] = (u32)page_table_kernel  | PAGE_PRESENT | PAGE_WRITE;
    page_directory[1] = (u32)page_table_kernel2 | PAGE_PRESENT | PAGE_WRITE;

    asm volatile(
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or  $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        : : "r"(page_directory) : "eax"
    );
}

void map_user_page(u32 virt, u32 phys)
{
    u32 dir_idx   = virt >> 22;
    u32 table_idx = (virt >> 12) & 0x3FF;

    if (!(page_directory[dir_idx] & PAGE_PRESENT)) {
        u32 new_table = alloc_page_frame();
        page_directory[dir_idx] = new_table | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }

    u32 *table = (u32*)(page_directory[dir_idx] & ~0xFFF);
    table[table_idx] = phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
}


/* =========================================================
 * GDT
 * ========================================================= */

void init_gdt_descriptor(u32 base, u32 limite, u8 acces, u8 other, struct GDT_entry *entry)
{
    entry->lim0_15   = (limite & 0xffff);
    entry->base0_15  = (base   & 0xffff);
    entry->base16_23 = (base   & 0xff0000)   >> 16;
    entry->acces     = acces;
    entry->lim16_19  = (limite & 0xf0000)    >> 16;
    entry->other     = (other  & 0xf);
    entry->base24_31 = (base   & 0xff000000) >> 24;
}

void gdt_init(void)
{
    default_tss.io_map = 0x00;
    default_tss.esp0   = 0x1FFF0;
    default_tss.ss0    = 0x10;

    /* Segment descriptors */
    init_gdt_descriptor(0x0,             0x0,     0x00, 0x00, &kgdt[0]); /* null        */
    init_gdt_descriptor(0x0,             0xFFFFF, 0x9B, 0x0D, &kgdt[1]); /* kernel code */
    init_gdt_descriptor(0x0,             0xFFFFF, 0x93, 0x0D, &kgdt[2]); /* kernel data */
    init_gdt_descriptor(0x0,             0x0,     0x97, 0x0D, &kgdt[3]); /* kernel stack */
    init_gdt_descriptor(0x0,             0xFFFFF, 0xFF, 0x0D, &kgdt[4]); /* user code   */
    init_gdt_descriptor(0x0,             0xFFFFF, 0xF3, 0x0D, &kgdt[5]); /* user data   */
    init_gdt_descriptor(0x0,             0xFFFFF, 0xF7, 0x0D, &kgdt[6]); /* user stack  */
    init_gdt_descriptor((u32)&default_tss, 0x67,  0xE9, 0x00, &kgdt[7]); /* TSS         */

    kgdtr.limite = sizeof(kgdt) - 1;
    kgdtr.base   = GDTBASE;

    memcpy((void*)GDTBASE, (void*)kgdt, sizeof(kgdt));

    asm volatile("lgdt %0" : : "m"(kgdtr));

    /* Load segment registers (written here instead of the bootloader because life is short) */
    asm volatile(
        "movw $0x10, %ax\n"
        "movw %ax, %ds\n"
        "movw %ax, %es\n"
        "movw %ax, %fs\n"
        "movw %ax, %gs\n"
        "ljmp $0x08, $1f\n"
        "1:\n"
    );

    asm volatile("ltr %%ax" : : "a"(0x38)); /* load TSS */
}


/* =========================================================
 * IDT
 * ========================================================= */

void idt_init(void)
{
    extern void syscall_handler(void);

    unsigned long keyboard_address = (unsigned long)kbd_handler;
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

    IDT[0x80].offset_lowerbits  = ((u32)syscall_handler) & 0xffff;
    IDT[0x80].selector          = KERNEL_CODE_SEGMENT_OFFSET;
    IDT[0x80].zero              = 0;
    IDT[0x80].type_attr         = 0xEF;
    IDT[0x80].offset_higherbits = ((u32)syscall_handler >> 16);

    /* PIC initialisation (8259A)
     * ICW1 - begin init */
    write_port(0x20, 0x11);
    write_port(0xA0, 0x11);

    /* ICW2 - vector offsets */
    write_port(0x21, 0x20);
    write_port(0xA1, 0x28);

    /* ICW3 - cascading */
    write_port(0x21, 0x04);
    write_port(0xA1, 0x02);

    /* ICW4 - environment info */
    write_port(0x21, 0x01);
    write_port(0xA1, 0x01);

    /* Mask all except keyboard (IRQ1) and timer (IRQ0) */
    write_port(0x21, 0xFC);
    write_port(0xA1, 0xFE);

    unsigned long idt_address = (unsigned long)IDT;
    unsigned long idt_ptr[2];
    idt_ptr[0] = (sizeof(struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
    idt_ptr[1] = idt_address >> 16;

    load_idt(idt_ptr);
}


/* =========================================================
 * PIT (timer)
 * ========================================================= */

void pit_init(void)
{
    unsigned short divisor = PIT_FREQ / HZ;
    write_port(0x43, 0x36);
    write_port(0x40, divisor & 0xFF);
    write_port(0x40, divisor >> 8);
}

void timer_callback(void)
{
    jiffies++;
    write_port(0x20, 0x20); /* send EOI */
}

uint32_t uptime(void)
{
    return jiffies / HZ;
}


/* =========================================================
 * Userspace entry points
 * ========================================================= */

void jump_to_userspace(void)
{
    asm volatile("mov %%esp, %0" : "=r"(kernel_esp_save));
    asm volatile(
        "mov $0x2B, %%ax\n"     /* user data segment   */
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "push $0x33\n"          /* SS  = user stack    */
        "push $0xB0000FF0\n"    /* ESP                 */
        "pushf\n"               /* EFLAGS              */
        "push $0x23\n"          /* CS  = user code     */
        "push $0xA0000020\n"    /* EIP                 */
        "iret\n"
        : : : "eax"
    );
}

void jump_to_editor(void)
{
    asm volatile("mov %%esp, %0" : "=r"(kernel_esp_save));
    asm volatile(
        "mov $0x2B, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "push $0x33\n"
        "push $0x71000FF0\n"
        "pushf\n"
        "push $0x23\n"
        "push $0x70000000\n"
        "iret\n"
        : : : "eax"
    );
}

void jump_to_shell(void)
{
    asm volatile("mov %%esp, %0" : "=r"(kernel_esp_save));
    asm volatile(
        "mov $0x2B, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "push $0x33\n"
        "push $0xD0000FF0\n"
        "pushf\n"
        "push $0x23\n"
        "push $0xC0000000\n"
        "iret\n"
        : : : "eax"
    );
}


/* =========================================================
 * Syscall dispatch
 * ========================================================= */

u32 syscall_dispatch(u32 syscall_num, u32 arg1, u32 arg2, u32 arg3)
{
    switch (syscall_num) {
        case 0:  exit_requested = 1;                              return 0;
        case 1:  kprintf("%s", (char*)arg1);                      return 0;
        case 2:  return (u32)kbd_getchar();
        case 3:  return (u32)malloc((size_t)arg1);
        case 4:  mem_free((void*)arg1);                           return 0;
        case 5:  fs_list_files();                                 return 0;
        case 6:  fs_create_file((char*)arg1);                     return 0;
        case 7:  fs_delete_file((char*)arg1);                     return 0;
        case 8:  return (u32)fs_read_file((char*)arg1, (char*)arg2, (u32)arg3);
        case 9:  return (u32)fs_write_file((char*)arg1, (char*)arg2, (u32)arg3);
        case 10:
            /* Backspace: erase the last character from the VGA buffer */
            if (current_loc >= 2) {
                current_loc -= 2;
                vidptr[current_loc]     = ' ';
                vidptr[current_loc + 1] = 0x07;
            }
            return 0;
        case 11: clear_screen();                                  return 0;
        case 12: {
            /*
             * _sbrk: bump a per-process heap pointer, mapping new
             * physical pages as the pointer crosses page boundaries.
             */
            static u32 user_heap_ptr = 0xE0000000;
            u32 old     = user_heap_ptr;
            u32 new_ptr = old + arg1;
            for (u32 addr = old & ~0xFFF; addr < new_ptr; addr += 0x1000)
                map_user_page(addr, alloc_page_frame());
            user_heap_ptr = new_ptr;
            return old;
        }
        case 13: jump_to_editor();                                return 0;
        case 14: kvga_write_char((int)arg1, (int)arg2, (char)(arg3 & 0xFF), (uint8_t)(arg3 >> 8)); return 0;
        case 15: kvga_set_cursor((int)arg1, (int)arg2);           return 0;
        case 16: kvga_clear_to_eol((int)arg1, (int)arg2);         return 0;
        default:
            kprintf("unknown syscall %d\n", syscall_num);
            return 0xFFFFFFFF;
    }
}


/* =========================================================
 * kernel_main
 * ========================================================= */

/*
 * Helper: copy a flat binary blob into a pre-mapped virtual address.
 */
static void copy_binary(u8 *dst, char *start, char *end)
{
    int i = 0;
    while (start < end)
        dst[i++] = *start++;
}

/*
 * Helper: zero a region of memory (poor man's memset before mm is up).
 */
static void zero_region(u8 *ptr, int bytes)
{
    for (int i = 0; i < bytes; i++)
        ptr[i] = 0;
}

void kernel_main(void)
{
    clear_screen();
    gdt_init();
    paging_alloc_init();
    init_paging();

    /* --- User test binary @ 0xA0000000 --- */
    map_user_page(0xA0000000, alloc_page_frame()); /* code  */
    map_user_page(0xB0000000, alloc_page_frame()); /* stack */
    copy_binary((u8*)0xA0000000, _binary_user_flat_start, _binary_user_flat_end);

    /* --- Shared IPC page @ 0xF0000000 --- */
    map_user_page(0xF0000000, alloc_page_frame());
    zero_region((u8*)0xF0000000, 4096);

    /* --- Editor binary @ 0x70000000 (12 code pages + 1 stack page) --- */
    for (int p = 0; p < 12; p++)
        map_user_page(0x70000000 + p * 0x1000, alloc_page_frame());
    map_user_page(0x71000000, alloc_page_frame());
    zero_region((u8*)0x70000000, 12 * 4096);
    copy_binary((u8*)0x70000000, _binary_user_editor_flat_start, _binary_user_editor_flat_end);

    /* --- Shell binary @ 0xC0000000 (12 code pages + 1 stack page) --- */
    for (int p = 0; p < 12; p++)
        map_user_page(0xC0000000 + p * 0x1000, alloc_page_frame());
    map_user_page(0xD0000000, alloc_page_frame());
    zero_region((u8*)0xC0000000, 12 * 4096); /* also initialises BSS */
    copy_binary((u8*)0xC0000000, _binary_user_shell_flat_start, _binary_user_shell_flat_end);

    /* --- Hardware init --- */
    idt_init();
    pit_init();
    write_port(0x21, read_port(0x21) & ~0x01); /* unmask IRQ0 (timer) */
    asm volatile("sti");
    kbd_init();
    kbd_enable();

    /* --- Software init --- */
    init_dynamic_mem();
    fs_init();
    initShell();
    lsh_loop();

    while (1); /* should never reach here */
}
