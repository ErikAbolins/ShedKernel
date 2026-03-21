SECTIONS {
    . = 0x70000000;
    .text : {
        *(.text._start)
        *(.text*)
    }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }
}
