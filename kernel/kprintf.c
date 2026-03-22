/*
 * kprintf.c - VGA text-mode output
 *
 * Covers: kprint, kprintf, ksprintf, and VGA cursor/cell helpers
 *         used by the editor subsystem.
 */

#include "kprintf.h"
#include <stdarg.h>
#include <stdint.h>

char        *vidptr      = (char *)0xb8000;
unsigned int current_loc = 0;


/* =========================================================
 * Internal helpers
 * ========================================================= */

/*
 * Write the decimal/hex/octal representation of num (in the given base)
 * into buf[], working backwards from buf_end. Returns a pointer to the
 * start of the string within buf[]. buf_end must point to the last byte
 * of a caller-supplied buffer large enough to hold the result + NUL.
 */
static char *itoa_into(unsigned int num, int base, char *buf_end)
{
    static const char digits[] = "0123456789ABCDEF";
    char *p = buf_end;
    *p = '\0';
    do {
        *--p = digits[num % base];
        num /= base;
    } while (num != 0);
    return p;
}

/*
 * Same conversion but into a shared static buffer — safe for kprintf's
 * VGA path where we don't need re-entrancy between format specifiers.
 */
static char *itoa_static(unsigned int num, int base)
{
    static char buf[50];
    return itoa_into(num, base, &buf[49]);
}


/* =========================================================
 * VGA editor helpers
 * ========================================================= */

/* Position the VGA cursor (0-indexed row, col). */
void kvga_set_cursor(int row, int col)
{
    current_loc = (row * COLUMNS_IN_LINE + col) * BYTES_FOR_EACH_ELEMENT;
}

/* Blank every cell from (row, col) to end of that row. */
void kvga_clear_to_eol(int row, int col)
{
    unsigned int loc = (row * COLUMNS_IN_LINE + col)              * BYTES_FOR_EACH_ELEMENT;
    unsigned int end = (row * COLUMNS_IN_LINE + COLUMNS_IN_LINE)  * BYTES_FOR_EACH_ELEMENT;
    while (loc < end) {
        vidptr[loc++] = ' ';
        vidptr[loc++] = 0x07;
    }
}

/* Write a single character with an explicit attribute byte (e.g. 0x70 for reversed video). */
void kvga_write_char(int row, int col, char c, uint8_t attr)
{
    unsigned int loc = (row * COLUMNS_IN_LINE + col) * BYTES_FOR_EACH_ELEMENT;
    vidptr[loc]     = c;
    vidptr[loc + 1] = attr;
}


/* =========================================================
 * Core output
 * ========================================================= */

void kprint(const char *str)
{
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
    current_loc = 0;
}

void kprint_hex(uint32_t val)
{
    char buf[11] = "0x00000000";
    const char hex[] = "0123456789ABCDEF";
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    kprint(buf);
    kprint_newline();
}


/* =========================================================
 * kprintf — formatted output to VGA
 * ========================================================= */

void kprintf(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);

    for (const char *p = format; *p != '\0'; p++) {
        if (*p == '\n') {
            kprint_newline();
            continue;
        }

        if (*p != '%') {
            vidptr[current_loc++] = *p;
            vidptr[current_loc++] = 0x07;
            continue;
        }

        p++;

        /* Optional precision: %.N or *.s */
        int precision = -1;
        if (*p == '.') {
            p++;
            if (*p == '*') {
                precision = va_arg(arg, int);
                p++;
            } else {
                precision = 0;
                while (*p >= '0' && *p <= '9')
                    precision = precision * 10 + (*p++ - '0');
            }
        }

        char *s;
        unsigned int i;

        switch (*p) {
            case 'c':
                i = va_arg(arg, int);
                vidptr[current_loc++] = (char)i;
                vidptr[current_loc++] = 0x07;
                break;

            case 'd':
                i = va_arg(arg, int);
                if ((int)i < 0) {
                    i = -(int)i;
                    vidptr[current_loc++] = '-';
                    vidptr[current_loc++] = 0x07;
                }
                s = itoa_static(i, 10);
                while (*s) { vidptr[current_loc++] = *s++; vidptr[current_loc++] = 0x07; }
                break;

            case 'o':
                i = va_arg(arg, unsigned int);
                s = itoa_static(i, 8);
                while (*s) { vidptr[current_loc++] = *s++; vidptr[current_loc++] = 0x07; }
                break;

            case 's':
                s = va_arg(arg, char *);
                while (*s) {
                    if (*s == '\n') { kprint_newline(); s++; }
                    else            { vidptr[current_loc++] = *s++; vidptr[current_loc++] = 0x07; }
                }
                break;

            case 'x':
                i = va_arg(arg, unsigned int);
                s = itoa_static(i, 16);
                while (*s) { vidptr[current_loc++] = *s++; vidptr[current_loc++] = 0x07; }
                break;

            case '%':
                vidptr[current_loc++] = '%';
                vidptr[current_loc++] = 0x07;
                break;
        }
    }

    va_end(arg);
}


/* =========================================================
 * ksprintf — formatted output to a string buffer
 * ========================================================= */

int ksprintf(char *buf, const char *format, ...)
{
    char *out = buf;
    va_list arg;
    va_start(arg, format);

    for (const char *p = format; *p != '\0'; p++) {
        if (*p == '\n') { *out++ = '\n'; continue; }

        if (*p != '%') { *out++ = *p; continue; }

        p++;

        /* Optional precision */
        int precision = -1;
        if (*p == '.') {
            p++;
            if (*p == '*') {
                precision = va_arg(arg, int);
                p++;
            } else {
                precision = 0;
                while (*p >= '0' && *p <= '9')
                    precision = precision * 10 + (*p++ - '0');
            }
        }

        char tmp[50];
        char *s;
        unsigned int i;

        switch (*p) {
            case 'c':
                *out++ = (char)va_arg(arg, int);
                break;

            case 'd':
                i = va_arg(arg, int);
                if ((int)i < 0) { i = -(int)i; *out++ = '-'; }
                s = itoa_into(i, 10, &tmp[49]);
                while (*s) *out++ = *s++;
                break;

            case 'o':
                i = va_arg(arg, unsigned int);
                s = itoa_into(i, 8, &tmp[49]);
                while (*s) *out++ = *s++;
                break;

            case 's':
                s = va_arg(arg, char *);
                if (precision >= 0) {
                    for (int n = 0; *s && n < precision; n++) *out++ = *s++;
                } else {
                    while (*s) *out++ = *s++;
                }
                break;

            case 'x':
                i = va_arg(arg, unsigned int);
                s = itoa_into(i, 16, &tmp[49]);
                while (*s) *out++ = *s++;
                break;

            case '%':
                *out++ = '%';
                break;
        }
    }

    *out = '\0';
    va_end(arg);
    return (int)(out - buf);
}
