cmake_minimum_required(VERSION 3.20)
project(OSAttempt C ASM_NASM)

enable_language(ASM_NASM)

set(CMAKE_C_COMPILER i686-linux-gnu-gcc)

set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -f elf32 -o <OBJECT> <SOURCE>")

add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/user_bin.o
        COMMAND i686-linux-gnu-gcc -m32 -nostdlib -nostdinc -ffreestanding
        -fno-pic -fno-pie -no-pie -nostartfiles
        -T ${CMAKE_SOURCE_DIR}/user.ld
        -o ${CMAKE_SOURCE_DIR}/user.elf ${CMAKE_SOURCE_DIR}/user.c
        COMMAND i686-linux-gnu-objcopy -O binary
        --gap-fill 0x00
        ${CMAKE_SOURCE_DIR}/user.elf ${CMAKE_SOURCE_DIR}/user.flat
        COMMAND cd ${CMAKE_SOURCE_DIR} && i686-linux-gnu-objcopy -I binary -O elf32-i386 -B i386
        user.flat user_bin.o
        DEPENDS ${CMAKE_SOURCE_DIR}/user.c
        COMMENT "Building userspace binary"
)

add_custom_target(user_binary DEPENDS ${CMAKE_SOURCE_DIR}/user_bin.o)

add_executable(kernel.elf boot.asm kernel.c
        keyboard_map.h
        kbd_driver.c
        kbd_driver.h
        mm.h
        mm.c
        time.c
        time.h
        kprintf.c
        kprintf.h
        Shell.c
        Shell.h
        string.c
        string.h
        easyfs.c
        easyfs.h
        fs_config.h
        fs_types.h
        ed.c
        ed.h
)

add_dependencies(kernel.elf user_binary)

target_compile_options(kernel.elf PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
        $<$<COMPILE_LANGUAGE:C>:-O2>
        $<$<COMPILE_LANGUAGE:C>:-nostdlib>
)

target_link_options(kernel.elf PRIVATE
        -nostdlib
        -e _start
        -T ${CMAKE_SOURCE_DIR}/linker.ld
        ${CMAKE_SOURCE_DIR}/user_bin.o
)