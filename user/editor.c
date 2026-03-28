#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shed.h"

__attribute__((section(".text._start"))) void _start(void) {
    printf("shed editor\n");
    sys_exit();
}



