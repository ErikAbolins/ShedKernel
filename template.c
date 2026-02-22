#include "template.h"

extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);

static driver_t state = {
    .initialized = 0,
    .enabled = 0,
};


//helper
static void driver_reset(void) {
    //reset if needed
}


//api
int driver_init(void) {
    if (state.initialized) return DRIVER_OK;

    driver_reset();
    //hardware init
    //write_port(DRIVER_PORT, 0XFF);

    state.initialized = 1;
    state.enabled = 0;
    return DRIVER_OK;
}


void driver_enable(void) {
    if (!state.initialized) return;

    //write_port(DRIVER_PORT, 0x01);

    state.enabled = 1;

}


void driver_disable(void) {
    //write_port(DRIVER_PORT, 0x00);

    state.enabled = 0;
}


int driver_read(u8 *out) {
    if (!state.enabled) return DRIVER_ERR;

    //check status before reading port

    (void)out; //remove
    return DRIVER_OK;
}


int driver_write(u8 data) {
    if (!state.enabled) return DRIVER_ERR;

    //write_port(DRIVER_PORT, data);
    (void)data; //remove
    return DRIVER_OK;
}



void driver_handler(void) {
    write_port(0x20, 0x20); //send EOI

    if (!state.enabled) return;

    //read and handle
    /*
     *if (driver_read(&data) == DRIVER_OK) {
     *    //do shit
     *}
     */
}
