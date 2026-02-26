ENTRY(_start)
SECTIONS {
    . = 0x100000;
    .multiboot : { *(.multiboot) }
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data) }
    .bss : {
        bss_start = .;
        *(.bss)
        bss_end = .;
    }
    /DISCARD/ : { *(.eh_frame) *(.comment) }
}