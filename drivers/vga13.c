#include <stdint.h>
#include "vga13.h"
#include "mm.h"

static uint32_t  *fb       = 0;
static uint32_t  fb_pitch = 0;
static uint32_t  fb_width = 0;
static uint32_t  fb_height = 0;



void vga13_init(uint32_t phys_addr, uint32_t pitch, uint32_t width, uint32_t height)
{
    fb_pitch  = pitch;
    fb_width  = width;
    fb_height = height;
    fb        = (uint32_t*)phys_addr;
}

void vga13_put_pixel(int x, int y, uint32_t colour)
{
    uint32_t pixels_per_row = fb_pitch / 4;
    fb[y * pixels_per_row + x] = colour;
}

void vga13_clear(uint32_t colour)
{
    uint32_t pixels_per_row = fb_pitch / 4;
    uint32_t total_pixels = pixels_per_row * fb_height;

    for (uint32_t i = 0; i < total_pixels; i++) fb[i] = colour;
}
