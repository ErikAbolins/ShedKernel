#include "kbd_driver.h"
#include "keyboard_map.h"



extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void kprint_newline(void);

extern unsigned int current_loc;
extern char *vidptr;

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

    if (kbd_read(&keycode) != DRIVER_OK)
        return;

    if (keycode & 0x80)
        return;

    if (keycode == ENTER_KEY_CODE) {
        kprint_newline();
        return;
    }

    vidptr[current_loc++] = keyboard_map[keycode];
    vidptr[current_loc++] = 0x07;
}