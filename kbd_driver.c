#include "kbd_driver.h"
#include "keyboard_map.h"
#include "mm.h"

#define KBD_BUF_SIZE 256

/* Scancodes */
#define SCANCODE_UP       0x48
#define SCANCODE_DOWN     0x50
#define SCANCODE_LEFT     0x4B
#define SCANCODE_RIGHT    0x4D
#define SCANCODE_HOME     0x47
#define SCANCODE_END      0x4F
#define SCANCODE_PGUP     0x49
#define SCANCODE_PGDN     0x51
#define SCANCODE_DEL      0x53
#define SCANCODE_LSHIFT   0x2A
#define SCANCODE_RSHIFT   0x36
#define SCANCODE_LSHIFT_R 0xAA  /* release */
#define SCANCODE_RSHIFT_R 0xB6  /* release */

static volatile int kbd_buf[KBD_BUF_SIZE];
static volatile int kbd_buf_read  = 0;
static volatile int kbd_buf_write = 0;
static volatile int shift_held    = 0;

extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern unsigned int current_loc;
extern char *vidptr;

static driver_t state = { .initialized = 0, .enabled = 0 };

static void kbd_push(int code) {
    kbd_buf[kbd_buf_write] = code;
    kbd_buf_write = (kbd_buf_write + 1) % KBD_BUF_SIZE;
}

int kbd_getchar(void) {
    while (kbd_buf_read == kbd_buf_write);
    int c = kbd_buf[kbd_buf_read];
    kbd_buf_read = (kbd_buf_read + 1) % KBD_BUF_SIZE;
    return c;
}

static void kbd_reset(void) { }

int kbd_init(void) {
    if (state.initialized) return DRIVER_OK;
    kbd_reset();
    write_port(0x21, 0xFD);
    state.initialized = 1;
    state.enabled = 0;
    return DRIVER_OK;
}

void kbd_enable(void)  { if (!state.initialized) return; state.enabled = 1; }
void kbd_disable(void) { state.enabled = 0; }

int kbd_read(u8 *out) {
    if (!state.enabled || !out) return DRIVER_ERR;
    if (!(read_port(KEYBOARD_STATUS_PORT) & 0x01)) return DRIVER_ERR;
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

    write_port(0x20, 0x20); /* EOI */

    if (!state.enabled) return;
    if (kbd_read(&keycode) != DRIVER_OK) return;

    /* Track shift state — these are the only key-release events we care about */
    if (keycode == SCANCODE_LSHIFT_R || keycode == SCANCODE_RSHIFT_R) {
        shift_held = 0;
        return;
    }

    if (keycode & 0x80) return; /* other key releases, ignore */

    /* Shift press */
    if (keycode == SCANCODE_LSHIFT || keycode == SCANCODE_RSHIFT) {
        shift_held = 1;
        return;
    }

    /* Special scancodes */
    switch (keycode) {
        case SCANCODE_UP:    kbd_push(ARROW_UP);      return;
        case SCANCODE_DOWN:  kbd_push(ARROW_DOWN);    return;
        case SCANCODE_LEFT:  kbd_push(ARROW_LEFT);    return;
        case SCANCODE_RIGHT: kbd_push(ARROW_RIGHT);   return;
        case SCANCODE_HOME:  kbd_push(KEY_HOME);      return;
        case SCANCODE_END:   kbd_push(KEY_END);       return;
        case SCANCODE_PGUP:  kbd_push(KEY_PAGE_UP);   return;
        case SCANCODE_PGDN:  kbd_push(KEY_PAGE_DOWN); return;
        case SCANCODE_DEL:   kbd_push(KEY_DEL);       return;
        case BACKSPACE_KEY_CODE: kbd_push('\b');       return;
        case ENTER_KEY_CODE:     kbd_push('\n');       return;
        default: break;
    }

    /* Regular character — pick map based on shift state */
    char c = shift_held ? keyboard_map_shift[keycode] : keyboard_map[keycode];
    if (c) kbd_push((int)c);
}