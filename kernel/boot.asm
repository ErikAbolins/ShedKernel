bits 32

section .multiboot
align 4
    dd 0x1BADB002
    dd 0x00000007
    dd -(0x1BADB002 + 0x00000007)

    dd 0
    dd 0
    dd 0
    dd 0
    dd 0
    dd 0
    dd 640
    dd 480
    dd 32

section .text
global _start
global kbd_handler
global read_port
global write_port
global load_idt
global timer_handler
global syscall_handler

extern kernel_main
extern kbd_handler_main
extern timer_callback
extern bss_start
extern bss_end
extern syscall_dispatch
extern kernel_esp_save
extern exit_requested
extern lsh_loop


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

timer_handler:
    pusha
    call timer_callback
    popa
    iretd

syscall_handler:
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    pop eax
    pusha
    mov eax, [esp + 28]   ; syscall number
    mov ebx, [esp + 16]   ; arg1
    mov ecx, [esp + 24]   ; arg2
    mov edx, [esp + 20]   ; arg3
    push edx
    push ecx
    push ebx
    push eax
    call syscall_dispatch
    add esp, 16
    mov [esp + 28], eax   ; return value

    mov eax, [exit_requested]
    test eax, eax
    jnz .do_exit

    popa
    iretd

.do_exit:
    mov dword [exit_requested], 0
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, [kernel_esp_save]
    call lsh_loop
    hlt


_start:
    cli
    mov esp, 0x90000   ; stack first
    push ebx               ; arg2: multiboot info ptr
    push eax               ; arg1: multiboot magic (GRUB puts 0x2BADB002 in eax)

    ;zero BSS
    mov edi, bss_start
    mov ecx, bss_end
    sub ecx, edi
    xor eax, eax
    rep stosd

    call kernel_main
    hlt

section .bss
resb 8192
stack_space:
