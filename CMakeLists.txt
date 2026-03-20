cmake_minimum_required(VERSION 3.20)
project(OSAttempt C ASM_NASM)

enable_language(ASM_NASM)

set(CMAKE_C_COMPILER i686-linux-gnu-gcc)
set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -f elf32 -o <OBJECT> <SOURCE>")

# ── Userspace ─────────────────────────────────────────────────────────────────

add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/user_bin.o
        COMMAND i686-linux-gnu-gcc -m32 -nostdlib -nostdinc -ffreestanding
        -fno-pic -fno-pie -no-pie -nostartfiles
        -T ${CMAKE_SOURCE_DIR}/user/user.ld
        -o ${CMAKE_SOURCE_DIR}/user.elf ${CMAKE_SOURCE_DIR}/user/user.c
        COMMAND i686-linux-gnu-objcopy -O binary --gap-fill 0x00
        ${CMAKE_SOURCE_DIR}/user.elf ${CMAKE_SOURCE_DIR}/user.flat
        COMMAND cd ${CMAKE_SOURCE_DIR} && i686-linux-gnu-objcopy -I binary -O elf32-i386 -B i386
        user.flat user_bin.o
        DEPENDS ${CMAKE_SOURCE_DIR}/user/user.c
        COMMENT "Building userspace test binary"
)

add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/user_shell_bin.o
        COMMAND i686-linux-gnu-gcc -m32 -nostdlib -nostdinc -ffreestanding
        -fno-pic -fno-pie -no-pie -nostartfiles
        -T ${CMAKE_SOURCE_DIR}/user/user_shell.ld
        -o ${CMAKE_SOURCE_DIR}/user_shell.elf ${CMAKE_SOURCE_DIR}/user/user_shell.c
        COMMAND i686-linux-gnu-objcopy -O binary
        --gap-fill 0x00
        ${CMAKE_SOURCE_DIR}/user_shell.elf ${CMAKE_SOURCE_DIR}/user_shell.flat
        COMMAND cd ${CMAKE_SOURCE_DIR} && i686-linux-gnu-objcopy -I binary -O elf32-i386 -B i386
        user_shell.flat user_shell_bin.o
        DEPENDS ${CMAKE_SOURCE_DIR}/user/user_shell.c
        COMMENT "Building userspace shell binary"
)

add_custom_target(user_binary DEPENDS ${CMAKE_SOURCE_DIR}/user_bin.o)
add_custom_target(user_shell_binary DEPENDS ${CMAKE_SOURCE_DIR}/user_shell_bin.o)

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
        fs/easyfs.c
        fs/easyfs.h
        fs/fs_config.h
        fs/fs_types.h
        fs/ed.c
        fs/ed.h
)

add_dependencies(kernel.elf user_binary)
add_dependencies(kernel.elf user_shell_binary)

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
)