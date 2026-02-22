bits 32

section .multiboot
align 4
    dd 0x1BADB002
    dd 0x00
    dd -(0x1BADB002 + 0x00)

section .text
global _start
global kbd_handler
global read_port
global write_port
global load_idt

extern kernel_main
extern kbd_handler_main

read_port:
    mov edx, [esp + 4]
    in al, dx
    ret

write_port:
    mov edx, [esp + 4]
    mov al, [esp + 8]
    out dx, al
    ret

load_idt:
    mov edx, [esp + 4]
    lidt [edx]
    sti
    ret

kbd_handler:
    pusha
    call kbd_handler_main
    popa
    iretd

_start:
    cli
    mov esp, stack_space
    call kernel_main
    hlt

section .bss
resb 8192
stack_space: