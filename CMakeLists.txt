cmake_minimum_required(VERSION 3.20)
project(OSAttempt C ASM_NASM)

enable_language(ASM_NASM)

# Tell Cmake to use i686
set(CMAKE_C_COMPILER i686-linux-gnu-gcc)

# Set the object format for NASM
set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -f elf32 -o <OBJECT> <SOURCE>")
# Define the kernel target
add_executable(kernel.elf boot.asm kernel.c
        kernel.c
        keyboard_map.h
        #template.h
        #template.c
        kbd_driver.c
        kbd_driver.h
        mm.h
        mm.c
        kbd_driver.c
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
)

target_compile_options(kernel.elf PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
        $<$<COMPILE_LANGUAGE:C>:-O2>
        $<$<COMPILE_LANGUAGE:C>:-nostdlib>
)


target_link_options(kernel.elf PRIVATE
        -nostdlib
        -e _start
        -T ${CMAKE_SOURCE_DIR}/linker.ld
)