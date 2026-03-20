#include "shed.h"

void _start() {
    sys_print("hello from userspace!\n");
    sys_exit();
}