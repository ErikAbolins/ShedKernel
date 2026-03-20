#ifndef TEMPLAET_H
#define TEMPLAET_H


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

#define DRIVER_OK 0
#define DRIVER_ERR -1
#define DRIVER_BUSY -2
#define DRIVER_TIMEOUT -3


typedef struct {
    u8 initialized;
    u8 enabled;

    //driver specific states
    u32 state;
} driver_t;


//api
int driver_init(void);
int driver_enable(void);
int driver_disable(void);
int driver_read(u8 *out);
int driver_write(u8 data);
void driver_handler(void); //IRQ

#endif //TEMPLATE_H