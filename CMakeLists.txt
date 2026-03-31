cmake_minimum_required(VERSION 3.20)
project(OSAttempt C ASM_NASM)

enable_language(ASM_NASM)

set(CMAKE_C_COMPILER /home/erik/opt/cross/bin/i686-elf-gcc)
set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -f elf32 -o <OBJECT> <SOURCE>")

# ── Userspace ─────────────────────────────────────────────────────────────────

add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/user_bin.o
        COMMAND /home/erik/opt/cross/bin/i686-elf-gcc -m32 -nostdlib -nostdinc -ffreestanding
        -fno-pic -fno-pie -no-pie -nostartfiles
        -T ${CMAKE_SOURCE_DIR}/user/user.ld
        -o ${CMAKE_SOURCE_DIR}/user.elf ${CMAKE_SOURCE_DIR}/user/user.c
        COMMAND /home/erik/opt/cross/bin/i686-elf-objcopy -O binary --gap-fill 0x00
        ${CMAKE_SOURCE_DIR}/user.elf ${CMAKE_SOURCE_DIR}/user.flat
        COMMAND cd ${CMAKE_SOURCE_DIR} && /home/erik/opt/cross/bin/i686-elf-objcopy -I binary -O elf32-i386 -B i386
        user.flat user_bin.o
        DEPENDS ${CMAKE_SOURCE_DIR}/user/user.c
        COMMENT "Building userspace test binary"
)

add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/user_shell_bin.o
        COMMAND /home/erik/opt/cross/bin/i686-elf-gcc -m32 -ffreestanding -nostartfiles
        -fno-pic -fno-pie -no-pie
        -isystem /home/erik/opt/cross/i686-elf/include
        -T ${CMAKE_SOURCE_DIR}/user/user_shell.ld
        -o ${CMAKE_SOURCE_DIR}/user_shell.elf
        ${CMAKE_SOURCE_DIR}/user/user_shell.c
        ${CMAKE_SOURCE_DIR}/user/syscalls.c
        -L/home/erik/opt/cross/i686-elf/lib
        -lc -lm
        COMMAND /home/erik/opt/cross/bin/i686-elf-objcopy -O binary
        --gap-fill 0x00
        ${CMAKE_SOURCE_DIR}/user_shell.elf ${CMAKE_SOURCE_DIR}/user_shell.flat
        COMMAND cd ${CMAKE_SOURCE_DIR} && /home/erik/opt/cross/bin/i686-elf-objcopy -I binary -O elf32-i386 -B i386
        user_shell.flat user_shell_bin.o
        DEPENDS ${CMAKE_SOURCE_DIR}/user/user_shell.c ${CMAKE_SOURCE_DIR}/user/syscalls.c
        COMMENT "Building userspace shell binary"
)

add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/user_editor_bin.o
        COMMAND /home/erik/opt/cross/bin/i686-elf-gcc -m32 -ffreestanding -nostartfiles
        -fno-pic -fno-pie -no-pie
        -isystem /home/erik/opt/cross/i686-elf/include
        -T ${CMAKE_SOURCE_DIR}/user/user_editor.ld
        -o ${CMAKE_SOURCE_DIR}/user_editor.elf
        ${CMAKE_SOURCE_DIR}/user/editor.c
        ${CMAKE_SOURCE_DIR}/user/syscalls.c
        -L/home/erik/opt/cross/i686-elf/lib
        -lc -lm
        COMMAND /home/erik/opt/cross/bin/i686-elf-objcopy -O binary
        --gap-fill 0x00
        ${CMAKE_SOURCE_DIR}/user_editor.elf ${CMAKE_SOURCE_DIR}/user_editor.flat
        COMMAND cd ${CMAKE_SOURCE_DIR} && /home/erik/opt/cross/bin/i686-elf-objcopy -I binary -O elf32-i386 -B i386
        user_editor.flat user_editor_bin.o
        DEPENDS ${CMAKE_SOURCE_DIR}/user/editor.c ${CMAKE_SOURCE_DIR}/user/syscalls.c
        COMMENT "Building editor binary"
)

add_custom_target(user_binary DEPENDS ${CMAKE_SOURCE_DIR}/user_bin.o)
add_custom_target(user_shell_binary DEPENDS ${CMAKE_SOURCE_DIR}/user_shell_bin.o)
add_custom_target(user_editor_binary DEPENDS ${CMAKE_SOURCE_DIR}/user_editor_bin.o)

# ── Kernel ────────────────────────────────────────────────────────────────────

add_executable(kernel.elf
        kernel/boot.asm
        kernel/kernel.c
        kernel/mm.c
        kernel/mm.h
        kernel/time.c
        kernel/time.h
        kernel/kprintf.c
        kernel/kprintf.h
        kernel/string.c
        kernel/string.h
        kernel/Shell.c
        kernel/Shell.h
        drivers/kbd_driver.c
        drivers/kbd_driver.h
        drivers/keyboard_map.h
        drivers/acpi.h
        drivers/vga13.h
        drivers/vga13.c
        fs/easyfs.c
        fs/easyfs.h
        fs/fs_config.h
        fs/fs_types.h
        fs/ed.c
        fs/ed.h
)

add_dependencies(kernel.elf user_binary)
add_dependencies(kernel.elf user_shell_binary)
add_dependencies(kernel.elf user_editor_binary)

# ── ISO ───────────────────────────────────────────────────────────────────────

set(ISO_DIR ${CMAKE_SOURCE_DIR}/iso)
set(ISO_GRUB_DIR ${ISO_DIR}/boot/grub)
set(ISO_OUTPUT ${CMAKE_SOURCE_DIR}/shedkernel.iso)

file(MAKE_DIRECTORY ${ISO_GRUB_DIR})



add_custom_target(iso
    COMMAND cp ${CMAKE_BINARY_DIR}/kernel.elf ${ISO_DIR}/boot/kernel.elf
    COMMAND grub-mkrescue -o ${ISO_OUTPUT} ${ISO_DIR}
    DEPENDS kernel.elf
    COMMENT "Building ShedKernel ISO"
    VERBATIM
)

target_include_directories(kernel.elf PRIVATE
        kernel drivers fs user
)

target_compile_options(kernel.elf PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
        $<$<COMPILE_LANGUAGE:C>:-O2>
        $<$<COMPILE_LANGUAGE:C>:-nostdlib>
)

target_link_options(kernel.elf PRIVATE
        -nostdlib
        -e _start
        -T ${CMAKE_SOURCE_DIR}/linker/linker.ld
        ${CMAKE_SOURCE_DIR}/user_bin.o
        ${CMAKE_SOURCE_DIR}/user_shell_bin.o
        ${CMAKE_SOURCE_DIR}/user_editor_bin.o
)
