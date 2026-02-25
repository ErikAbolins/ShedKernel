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



#endif //KPRINTF_H