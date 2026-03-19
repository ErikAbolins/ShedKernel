#include "kprintf.h"
#include <stdarg.h>
#include <stdint.h>

char *vidptr = (char*)0xb8000;
unsigned int current_loc = 0;

/* ---------------------------------------------------------- */
/* Internal helpers                                            */
/* ---------------------------------------------------------- */

static char *convert_to_buf(unsigned int num, int base, char *buf_end) {
    static char Representation[] = "0123456789ABCDEF";
    char *ptr = buf_end;
    *ptr = '\0';
    do {
        *--ptr = Representation[num % base];
        num /= base;
    } while (num != 0);
    return ptr;
}

char *convert(unsigned int num, int base) {
    static char Representation[] = "0123456789ABCDEF";
    static char buf[50];
    char *ptr = &buf[49];
    *ptr = '\0';
    do {
        *--ptr = Representation[num % base];
        num /= base;
    } while (num != 0);
    return ptr;
}

/* ---------------------------------------------------------- */
/* VGA editor helpers                                          */
/* ---------------------------------------------------------- */

/*
 * Set current_loc to (row, col) so the next kprint/kprintf
 * call writes there. Both are 0-indexed.
 */
void kvga_set_cursor(int row, int col) {
    current_loc = (row * COLUMNS_IN_LINE + col) * BYTES_FOR_EACH_ELEMENT;
}

/*
 * Blank every cell from (row, col) to end of that row.
 * Leaves current_loc at start of next row.
 */
void kvga_clear_to_eol(int row, int col) {
    unsigned int loc = (row * COLUMNS_IN_LINE + col) * BYTES_FOR_EACH_ELEMENT;
    unsigned int end = (row * COLUMNS_IN_LINE + COLUMNS_IN_LINE) * BYTES_FOR_EACH_ELEMENT;
    while (loc < end) {
        vidptr[loc++] = ' ';
        vidptr[loc++] = 0x07;
    }
}

/*
 * Write a single character with explicit attribute byte at (row, col).
 * Useful for the status bar (reversed video = 0x70).
 */
void kvga_write_char(int row, int col, char c, uint8_t attr) {
    unsigned int loc = (row * COLUMNS_IN_LINE + col) * BYTES_FOR_EACH_ELEMENT;
    vidptr[loc]     = c;
    vidptr[loc + 1] = attr;
}

/* ---------------------------------------------------------- */
/* Original functions — unchanged                             */
/* ---------------------------------------------------------- */

void kprint(const char *str) {
    unsigned int i = 0;
    while (str[i] != '\0') {
        vidptr[current_loc++] = str[i++];
        vidptr[current_loc++] = 0x07;
    }
}

void kprint_newline(void) {
    unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
    current_loc = current_loc + (line_size - current_loc % line_size);
}

void clear_screen(void) {
    unsigned int i = 0;
    while (i < SCREENSIZE) {
        vidptr[i++] = ' ';
        vidptr[i++] = 0x07;
    }
    current_loc = 0;
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

        traverse++;

        /* Handle precision/width prefix e.g. %.*s, %.20s */
        int precision = -1;
        if (*traverse == '.') {
            traverse++;
            if (*traverse == '*') {
                precision = va_arg(arg, int);
                traverse++;
            } else {
                precision = 0;
                while (*traverse >= '0' && *traverse <= '9')
                    precision = precision * 10 + (*traverse++ - '0');
            }
        }

        switch (*traverse) {
            case 'c':
                i = va_arg(arg, int);
                vidptr[current_loc++] = (char)i;
                vidptr[current_loc++] = 0x07;
                break;

            case 'd':
                i = va_arg(arg, int);
                if ((int)i < 0) {
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
                if (precision >= 0) {
                    int n = 0;
                    while (*s && n < precision) {
                        vidptr[current_loc++] = *s++;
                        vidptr[current_loc++] = 0x07;
                        n++;
                    }
                } else {
                    while (*s) {
                        vidptr[current_loc++] = *s++;
                        vidptr[current_loc++] = 0x07;
                    }
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

            case '%':
                vidptr[current_loc++] = '%';
                vidptr[current_loc++] = 0x07;
                break;
        }
    }

    va_end(arg);
}

int ksprintf(char *buf, const char *format, ...) {
    char *traverse;
    unsigned int i;
    char *s;
    char *buf_start = buf;

    va_list arg;
    va_start(arg, format);

    for (traverse = (char*)format; *traverse != '\0'; traverse++) {
        if (*traverse == '\n') { *buf++ = '\n'; continue; }
        if (*traverse != '%') { *buf++ = *traverse; continue; }

        traverse++;

        int precision = -1;
        if (*traverse == '.') {
            traverse++;
            if (*traverse == '*') {
                precision = va_arg(arg, int);
                traverse++;
            } else {
                precision = 0;
                while (*traverse >= '0' && *traverse <= '9')
                    precision = precision * 10 + (*traverse++ - '0');
            }
        }

        switch (*traverse) {
            case 'c':
                i = va_arg(arg, int);
                *buf++ = (char)i;
                break;
            case 'd': {
                i = va_arg(arg, int);
                if ((int)i < 0) { i = -(int)i; *buf++ = '-'; }
                char numbuf[50];
                s = convert_to_buf(i, 10, &numbuf[49]);
                while (*s) *buf++ = *s++;
                break;
            }
            case 'o': {
                i = va_arg(arg, unsigned int);
                char octbuf[50];
                s = convert_to_buf(i, 8, &octbuf[49]);
                while (*s) *buf++ = *s++;
                break;
            }
            case 's':
                s = va_arg(arg, char*);
                if (precision >= 0) {
                    int n = 0;
                    while (*s && n < precision) { *buf++ = *s++; n++; }
                } else {
                    while (*s) *buf++ = *s++;
                }
                break;
            case 'x': {
                i = va_arg(arg, unsigned int);
                char hexbuf[50];
                s = convert_to_buf(i, 16, &hexbuf[49]);
                while (*s) *buf++ = *s++;
                break;
            }
            case '%':
                *buf++ = '%';
                break;
        }
    }

    *buf = '\0';
    va_end(arg);
    return (int)(buf - buf_start);
}