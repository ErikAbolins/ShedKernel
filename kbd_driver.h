#ifndef KBD_DRIVER_H
#define KBD_DRIVER_H


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

#define DRIVER_OK 0
#define DRIVER_ERR -1
#define DRIVER_BUSY -2
#define DRIVER_TIMEOUT -3

#define KEYBOARD_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define ENTER_KEY_CODE 0x1C
#define BACKSPACE_KEY_CODE 0x0E


extern unsigned char keyboard_map[128];


typedef struct {
    u8 initialized;
    u8 enabled;

    //driver specific states
    u32 state;
} driver_t;


//api
int kbd_init(void);
void kbd_enable(void);
void kbd_disable(void);
int kbd_read(u8 *out);
int kbd_write(u8 data);
int kbd_getchar(void);
void kbd_handler_main(void); //IRQ

#endif //TEMPLATE_H