#include "kbd_driver.h"
#include "keyboard_map.h"
#include "mm.h"

#define KBD_BUF_SIZE 256
static volatile char kbd_buf [KBD_BUF_SIZE];
static volatile int kbd_buf_read = 0;
static volatile int kbd_buf_write = 0;



extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void kprint_newline(void);
extern void kprint(const char *str);

extern unsigned int current_loc;
extern char *vidptr;


int kbd_getchar(void) {
    while (kbd_buf_read == kbd_buf_write);
    char c = kbd_buf[kbd_buf_read];
    kbd_buf_read = (kbd_buf_read + 1) % KBD_BUF_SIZE;
    return (int)c;
}

static driver_t state = {
    .initialized = 0,
    .enabled = 0,
};


static void kbd_reset(void) {
    /* reset if needed */
}


int kbd_init(void) {
    if (state.initialized) return DRIVER_OK;

    kbd_reset();

    write_port(0x21, 0xFD);

    state.initialized = 1;
    state.enabled = 0;
    return DRIVER_OK;
}


void kbd_enable(void) {
    if (!state.initialized) return;
    state.enabled = 1;
}


void kbd_disable(void) {
    state.enabled = 0;
}


int kbd_read(u8 *out) {
    if (!state.enabled) return DRIVER_ERR;
    if (!out) return DRIVER_ERR;

    if (!(read_port(KEYBOARD_STATUS_PORT) & 0x01))
        return DRIVER_ERR;

    *out = (u8)read_port(KEYBOARD_PORT);
    return DRIVER_OK;
}


int kbd_write(u8 data) {
    if (!state.enabled) return DRIVER_ERR;

    while (read_port(KEYBOARD_STATUS_PORT) & 0x02);
    write_port(KEYBOARD_PORT, data);
    return DRIVER_OK;
}


void kbd_handler_main(void) {
    u8 keycode;

    write_port(0x20, 0x20);
    if (!state.enabled) return;
    if (kbd_read(&keycode) != DRIVER_OK) return;
    if (keycode & 0x80) return;
    if (keycode == BACKSPACE_KEY_CODE) { /* your backspace code */ return; }
    if (keycode == ENTER_KEY_CODE) {
        kbd_buf[kbd_buf_write] = '\n';
        kbd_buf_write = (kbd_buf_write + 1) % KBD_BUF_SIZE;
        kprint_newline();
        return;
    }

    char c = keyboard_map[keycode];
    kbd_buf[kbd_buf_write] = c;
    kbd_buf_write = (kbd_buf_write + 1) % KBD_BUF_SIZE;

    vidptr[current_loc++] = c;
    vidptr[current_loc++] = 0x07;
}