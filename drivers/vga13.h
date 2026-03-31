#ifndef VGA13_H
#define VGA13_H

#include <stdint.h>


void vga13_init(uint32_t phys_addr, uint32_t pitch, uint32_t width, uint32_t height);
void vga13_put_pixel(int x, int y, uint32_t colour);
void vga13_clear(uint32_t colour);

#endif /* VGA13_H */
