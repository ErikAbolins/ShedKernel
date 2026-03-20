static char user_heap[4096];
static unsigned int heap_pos = 0;

static inline void sys_print(const char *s) {
    asm volatile("mov $1, %%eax\n int $0x80\n" : : "b"(s) : "eax");
}

static inline char sys_getchar() {
    int c;
    asm volatile("mov $2, %%eax\n int $0x80\n mov %%eax, %0\n"
        : "=r"(c) : : "eax");
    return (char)c;
}

static inline void sys_ls() {
    asm volatile("mov $5, %%eax\n int $0x80\n" : : : "eax");
}

static inline void sys_touch(const char *name) {
    asm volatile("mov $6, %%eax\n int $0x80\n" : : "b"(name) : "eax");
}

static inline void sys_del(const char *name) {
    asm volatile("mov $7, %%eax\n int $0x80\n" : : "b"(name) : "eax");
}

static inline int sys_read_file(const char *name, void *buf, unsigned int size) {
    int ret;
    asm volatile(
        "mov $8, %%eax\n int $0x80\n mov %%eax, %0\n"
        : "=r"(ret) : "b"(name), "c"(buf), "d"(size) : "eax"
    );
    return ret;
}

static inline int sys_write_file(const char *name, const void *buf, unsigned int size) {
    int ret;
    asm volatile(
        "mov $9, %%eax\n int $0x80\n mov %%eax, %0\n"
        : "=r"(ret) : "b"(name), "c"(buf), "d"(size) : "eax"
    );
    return ret;
}

static inline void sys_exit() {
    asm volatile("xor %%eax, %%eax\n int $0x80\n" : : : "eax");
}

static inline void sys_backspace() {
    asm volatile("mov $10, %%eax\n int $0x80\n" : : : "eax");
}

static inline void sys_clear() {
    asm volatile("mov $11, %%eax\n int $0x80\n" : : : "eax");
}


static inline void u_heap_reset() {
    heap_pos = 0;
}


static inline void* u_malloc(unsigned int size) {
    void *ptr = &user_heap[heap_pos];
    heap_pos += size;
    return ptr;
}

static inline void u_free(void *ptr) {
    (void)ptr; // no-op for now
}