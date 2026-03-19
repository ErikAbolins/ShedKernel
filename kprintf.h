#ifndef KPRINTF_H
#define KPRINTF_H

#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE * LINES

#include <stdint.h>

void kprint(const char *str);
void kprint_newline(void);
void clear_screen(void);
void kprint_hex(uint32_t val);
void kprintf(const char *format, ...);
char *convert(unsigned int num, int base);
int ksprintf(char *buf, const char *format, ...);

/* VGA helpers for the editor */
void kvga_set_cursor(int row, int col);   /* move write position, 0-indexed */
void kvga_clear_to_eol(int row, int col); /* blank from (row,col) to end of line */
void kvga_write_char(int row, int col, char c, uint8_t attr); /* write single char with attribute */

#endif /* KPRINTF_H */