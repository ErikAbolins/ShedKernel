static inline void sys_print(const char *s) {
    asm volatile(
        "mov $1, %%eax\n"
        "int $0x80\n"
        : : "b"(s)   // "b" constraint forces ebx
        : "eax"
    );
}

static inline char sys_getchar() {
    int c;
    asm volatile("mov $2, %%eax\n int $0x80\n mov %%eax, %0\n"
        : "=r"(c) : : "eax");
    return (char)c;
}

static inline void sys_exit() {
    asm volatile("xor %%eax, %%eax\n int $0x80\n" : : : "eax");
}

void _start() {
    sys_print("hello from userspace!\n");
    sys_exit();
}