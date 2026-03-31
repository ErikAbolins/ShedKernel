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
#include "vga13.h"


/* Forward-declare port I/O so serial helpers can use them */
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);

/* =========================================================
 * Serial debug (COM1) — works regardless of display mode
 * ========================================================= */

static void serial_write(char c)
{
    while (!(read_port(0x3F8 + 5) & 0x20));
    write_port(0x3F8, (unsigned char)c);
}

static void serial_print(const char *s)
{
    while (*s) {
        if (*s == '\n') serial_write('\r');
        serial_write(*s++);
    }
}

static void serial_hex(unsigned int val)
{
    const char *hex = "0123456789ABCDEF";
    serial_print("0x");
    for (int i = 28; i >= 0; i -= 4)
        serial_write(hex[(val >> i) & 0xF]);
}

/* =========================================================
 * Multiboot info struct (partial — only fields we need)
 * ========================================================= */

typedef struct {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
    u32 drives_length;
    u32 drives_addr;
    u32 config_table;
    u32 boot_loader_name;
    u32 apm_table;
    u32 vbe_control_info;
    u32 vbe_mode_info;
    u16 vbe_mode;
    u16 vbe_interface_seg;
    u16 vbe_interface_off;
    u16 vbe_interface_len;
    u64 framebuffer_addr;   /* physical address of framebuffer */
    u32 framebuffer_pitch;  /* bytes per row */
    u32 framebuffer_width;
    u32 framebuffer_height;
    u8  framebuffer_bpp;
    u8  framebuffer_type;
} __attribute__((packed)) multiboot_info_t;

/* Framebuffer info — filled in by kernel_main, used by vga13 driver */
uint32_t fb_phys_addr  = 0;
uint32_t fb_pitch      = 0;
uint32_t fb_width      = 0;
uint32_t fb_height     = 0;
uint8_t  fb_bpp        = 0;

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

/* read_port / write_port already declared above */
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

    /* Load segment registers */
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

    /* PIC initialisation (8259A) */
    write_port(0x20, 0x11);
    write_port(0xA0, 0x11);
    write_port(0x21, 0x20);
    write_port(0xA1, 0x28);
    write_port(0x21, 0x04);
    write_port(0xA1, 0x02);
    write_port(0x21, 0x01);
    write_port(0xA1, 0x01);
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
        "mov $0x2B, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "push $0x33\n"
        "push $0xB0000FF0\n"
        "pushf\n"
        "push $0x23\n"
        "push $0xA0000020\n"
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
            if (current_loc >= 2) {
                current_loc -= 2;
                vidptr[current_loc]     = ' ';
                vidptr[current_loc + 1] = 0x07;
            }
            return 0;
        case 11: clear_screen();                                  return 0;
        case 12: {
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

static void copy_binary(u8 *dst, char *start, char *end)
{
    int i = 0;
    while (start < end)
        dst[i++] = *start++;
}

static void zero_region(u8 *ptr, int bytes)
{
    for (int i = 0; i < bytes; i++)
        ptr[i] = 0;
}


void kernel_main(uint32_t mb_magic, uint32_t mb_addr)
{
    serial_print("[1] kernel_main entered\n");

    /* Parse framebuffer info before anything else */
    multiboot_info_t *mb = (multiboot_info_t*)mb_addr;
    serial_print("[2] mb ptr set\n");

    if (mb && (mb->flags & (1 << 12))) {
        fb_phys_addr = (uint32_t)mb->framebuffer_addr;
        fb_pitch     = mb->framebuffer_pitch;
        fb_width     = mb->framebuffer_width;
        fb_height    = mb->framebuffer_height;
        fb_bpp       = mb->framebuffer_bpp;
        serial_print("[3] fb fields parsed\n");
    } else {
        serial_print("[3] no fb in multiboot (bit 12 not set)\n");
    }

    serial_print("[3] flags="); serial_hex(mb ? mb->flags : 0);
    serial_print(" fb_addr="); serial_hex(fb_phys_addr);
    serial_print(" w="); serial_hex(fb_width);
    serial_print(" h="); serial_hex(fb_height);
    serial_print(" pitch="); serial_hex(fb_pitch);
    serial_print(" bpp="); serial_hex(fb_bpp);
    serial_print("\n");

    clear_screen();
    serial_print("[4] clear_screen done\n");

    gdt_init();
    serial_print("[5] gdt_init done\n");

    paging_alloc_init();
    serial_print("[6] paging_alloc_init done\n");

    init_paging();
    serial_print("[7] init_paging done\n");

    /* --- Init VGA 13H --- */
    if (fb_phys_addr) {
        serial_print("[8] mapping framebuffer...\n");
        uint32_t fb_size = fb_pitch * fb_height;
        serial_print("    fb_size="); serial_hex(fb_size); serial_print("\n");
        for (uint32_t off = 0; off < fb_size; off += 0x1000) {
            uint32_t addr    = fb_phys_addr + off;
            uint32_t dir_idx = addr >> 22;
            uint32_t tbl_idx = (addr >> 12) & 0x3FF;

            if (!(page_directory[dir_idx] & 1)) {
                u32 new_tbl = alloc_page_frame();
                u32 *t = (u32*)new_tbl;
                for (int i = 0; i < 1024; i++) t[i] = 0;
                page_directory[dir_idx] = new_tbl | 3;
            }
            u32 *tbl = (u32*)(page_directory[dir_idx] & ~0xFFF);
            tbl[tbl_idx] = addr | 3;
        }
        asm volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax");
        serial_print("[9] fb mapped, calling vga13_init\n");
        vga13_init(fb_phys_addr, fb_pitch, fb_width, fb_height);
        serial_print("[10] vga13_init done\n");
    } else {
        serial_print("[8] fb_phys_addr=0, skipping fb map\n");
    }

    serial_print("[11] mapping user pages\n");

    /* --- User test binary @ 0xA0000000 --- */
    map_user_page(0xA0000000, alloc_page_frame());
    map_user_page(0xB0000000, alloc_page_frame());
    copy_binary((u8*)0xA0000000, _binary_user_flat_start, _binary_user_flat_end);
    serial_print("[12] user binary mapped\n");

    /* --- Shared IPC page @ 0xF0000000 --- */
    map_user_page(0xF0000000, alloc_page_frame());
    zero_region((u8*)0xF0000000, 4096);
    serial_print("[13] IPC page mapped\n");

    /* --- Editor binary @ 0x70000000 --- */
    for (int p = 0; p < 12; p++)
        map_user_page(0x70000000 + p * 0x1000, alloc_page_frame());
    map_user_page(0x71000000, alloc_page_frame());
    zero_region((u8*)0x70000000, 12 * 4096);
    copy_binary((u8*)0x70000000, _binary_user_editor_flat_start, _binary_user_editor_flat_end);
    serial_print("[14] editor mapped\n");

    /* --- Shell binary @ 0xC0000000 --- */
    for (int p = 0; p < 12; p++)
        map_user_page(0xC0000000 + p * 0x1000, alloc_page_frame());
    map_user_page(0xD0000000, alloc_page_frame());
    zero_region((u8*)0xC0000000, 12 * 4096);
    copy_binary((u8*)0xC0000000, _binary_user_shell_flat_start, _binary_user_shell_flat_end);
    serial_print("[15] shell mapped\n");

    /* --- Hardware init --- */
    idt_init();
    serial_print("[16] idt_init done\n");
    pit_init();
    serial_print("[17] pit_init done\n");
    write_port(0x21, read_port(0x21) & ~0x01);
    asm volatile("sti");
    serial_print("[18] interrupts enabled\n");
    kbd_init();
    kbd_enable();
    serial_print("[19] kbd init done\n");

    /* --- Software init --- */
    init_dynamic_mem();
    serial_print("[20] heap init done\n");
    fs_init();
    serial_print("[21] fs init done\n");
    vga13_clear(0x00FF0000U);  /* bright red in 32bpp XRGB */
    serial_print("[22] vga13_clear done\n");
    vga13_put_pixel(10, 10, 0x00FFFFFFU);
    serial_print("[23] vga13_put_pixel done\n");
    initShell();
    lsh_loop();

    while (1);
}
