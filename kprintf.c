#include"kprintf.h"
#include <stdarg.h>
#include <stdint.h>

char *vidptr = (char*)0xb8000;
unsigned int current_loc = 0;



void kprint(const char *str) {
    unsigned int i = 0;
    while (str[i] != '\0') {
        vidptr[current_loc++] = str[i++];
        vidptr[current_loc++] = 0x07;
    }
}

void kprint_newline(void)
{
    unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
    current_loc = current_loc + (line_size - current_loc % line_size);
}

void clear_screen(void)
{
    unsigned int i = 0;
    while (i < SCREENSIZE) {
        vidptr[i++] = ' ';
        vidptr[i++] = 0x07;
    }
}

void kprint_hex(uint32_t val) {
    char buf[11] = "0x00000000";
    char hex[] = "0123456789ABCDEF";
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    kprint(buf);
    kprint_newline();
}

void kprintf(const char *format, ...) {
    char *traverse;
    unsigned int i;
    char *s;

    va_list arg;
    va_start(arg, format);

    for (traverse = (char*)format; *traverse != '\0'; traverse++) {
        if (*traverse == '\n') {
            kprint_newline();
            continue;
        }


        if (*traverse != '%') {
            vidptr[current_loc++] = *traverse;
            vidptr[current_loc++] = 0x07;
            continue;
        }


        traverse++; // skip the %

        switch (*traverse) {
            case 'c':
                i = va_arg(arg, int);
                vidptr[current_loc++] = i;
                vidptr[current_loc++] = 0x07;
                break;

            case 'd':
                i = va_arg(arg, int);
                if (i < 0) {
                    i = -i;
                    vidptr[current_loc++] = '-';
                    vidptr[current_loc++] = 0x07;
                }
                s = convert(i, 10);
                while (*s) {
                    vidptr[current_loc++] = *s++;
                    vidptr[current_loc++] = 0x07;
                }
                break;

            case 'o':
                i = va_arg(arg, unsigned int);
                s = convert(i, 8);
                while (*s) {
                    vidptr[current_loc++] = *s++;
                    vidptr[current_loc++] = 0x07;
                }
                break;

            case 's':
                s = va_arg(arg, char*);
                while (*s) {
                    vidptr[current_loc++] = *s++;
                    vidptr[current_loc++] = 0x07;
                }
                break;

            case 'x':
                i = va_arg(arg, unsigned int);
                s = convert(i, 16);
                while (*s) {
                    vidptr[current_loc++] = *s++;
                    vidptr[current_loc++] = 0x07;
                }
                break;
        }
    }
    va_end(arg);
}

char *convert(unsigned int num, int base) {
    static char Representation[] = "0123456789ABCDEF";
    static char buf[50];
    char *ptr;

    ptr = &buf[49];
    *ptr = '\0';

    do {
        *--ptr = Representation[num % base];
        num /= base;
    } while (num != 0);

    return ptr;
}