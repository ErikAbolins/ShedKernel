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

static inline void sys_exec_editor(const char *filename) {
    /* write filename into shared memory then jump to editor */
    char *shared = (char*)0xF0000000;
    const char *s = filename;
    int i = 0;
    while (*s) shared[i++] = *s++;
    shared[i] = '\0';
    asm volatile("mov $13, %%eax\n int $0x80\n" : : : "eax");
}

/* shared memory — both editor and shell can see this */
#define SHED_SHARED_MEM ((char*)0xF0000000)

/* VGA attribute bytes */
#define VGA_ATTR_NORMAL   0x07  /* white on black */
#define VGA_ATTR_INVERTED 0x70  /* black on white */
#define VGA_ATTR_GREEN    0x0A  /* bright green on black */
#define VGA_ROWS 25
#define VGA_COLS 80

static inline void sys_vga_write_char(int row, int col, char c, unsigned char attr) {
    unsigned int packed = ((unsigned int)attr << 8) | (unsigned char)c;
    asm volatile("mov $14, %%eax\n int $0x80\n"
        : : "b"(row), "c"(col), "d"(packed) : "eax");
}

static inline void sys_vga_set_cursor(int row, int col) {
    asm volatile("mov $15, %%eax\n int $0x80\n"
        : : "b"(row), "c"(col) : "eax");
}

static inline void sys_vga_clear_to_eol(int row, int col) {
    asm volatile("mov $16, %%eax\n int $0x80\n"
        : : "b"(row), "c"(col) : "eax");
}

static inline void sys_vga_write_str(int row, int col, const char *s, unsigned char attr) {
    while (*s) sys_vga_write_char(row, col++, *s++, attr);
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