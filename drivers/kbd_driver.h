#ifndef KBD_DRIVER_H
#define KBD_DRIVER_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;

#define DRIVER_OK      0
#define DRIVER_ERR    -1
#define DRIVER_BUSY   -2
#define DRIVER_TIMEOUT -3

#define KEYBOARD_PORT        0x60
#define KEYBOARD_STATUS_PORT 0x64
#define ENTER_KEY_CODE       0x1C
#define BACKSPACE_KEY_CODE   0x0E

/*
 * Synthetic key codes — values above ASCII range so they
 * can live in the same int ring buffer as regular chars.
 * ed.h and kbd_driver.h must agree on these values.
 */
#define ARROW_UP    1000
#define ARROW_DOWN  1001
#define ARROW_LEFT  1002
#define ARROW_RIGHT 1003
#define KEY_HOME    1004
#define KEY_END     1005
#define KEY_PAGE_UP 1006
#define KEY_PAGE_DOWN 1007
#define KEY_DEL     1008

extern unsigned char keyboard_map[128];

typedef struct {
    u8 initialized;
    u8 enabled;
    u32 state;
} driver_t;

int  kbd_init(void);
void kbd_enable(void);
void kbd_disable(void);
int  kbd_read(u8 *out);
int  kbd_write(u8 data);
int  kbd_getchar(void);
void kbd_handler_main(void);

#endif /* KBD_DRIVER_H */