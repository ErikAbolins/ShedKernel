#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>

/* ── syscall numbers ─────────────────────────────────────────── */
#define SYS_EXIT        0
#define SYS_PRINT       1
#define SYS_GETCHAR     2
#define SYS_MALLOC      3
#define SYS_FREE        4
#define SYS_LS          5
#define SYS_TOUCH       6
#define SYS_DEL         7
#define SYS_READ_FILE   8
#define SYS_WRITE_FILE  9
#define SYS_BACKSPACE   10
#define SYS_CLEAR       11
#define SYS_SBRK        12

/* ── raw syscall helper ──────────────────────────────────────────────────── */
static inline uint32_t syscall3(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2), "d"(a3)
    );
    return ret;
}

/* ── _sbrk ───────────────────────────────────────────────────────────────── */
void *_sbrk(int incr) {
    void *ret = (void*)syscall3(SYS_SBRK, (uint32_t)incr, 0, 0);
    if ((uint32_t)ret == 0xFFFFFFFF) {
        errno = ENOMEM;
        return (void*)-1;
    }
    return ret;
}

/* ── _write ──────────────────────────────────────────────────────────────── */
int _write(int fd, char *buf, int len) {
    (void)fd;
    for (int i = 0; i < len; i++) {
        char tmp[2] = { buf[i], '\0' };
        syscall3(SYS_PRINT, (uint32_t)tmp, 0, 0);
    }
    return len;
}

/* ── _read ───────────────────────────────────────────────────────────────── */
int _read(int fd, char *buf, int len) {
    (void)fd;
    for (int i = 0; i < len; i++) {
        buf[i] = (char)syscall3(SYS_GETCHAR, 0, 0, 0);
        if (buf[i] == '\n') return i + 1;
    }
    return len;
}

/* ── _open / _close ──────────────────────────────────────────────────────── */
int _open(const char *name, int flags, int mode) {
    (void)flags; (void)mode;
    syscall3(SYS_TOUCH, (uint32_t)name, 0, 0);
    return 3;
}

int _close(int fd) {
    (void)fd;
    return 0;
}

/* ── _exit ───────────────────────────────────────────────────────────────── */
void _exit(int status) {
    (void)status;
    syscall3(SYS_EXIT, 0, 0, 0);
    while(1);
}

/* ── stubs ───────────────────────────────────────────────────────────────── */
int _fstat(int fd, struct stat *st) {
    (void)fd;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int fd) {
    (void)fd;
    return 1;
}

int _lseek(int fd, int offset, int whence) {
    (void)fd; (void)offset; (void)whence;
    return 0;
}

int _kill(int pid, int sig) {
    (void)pid; (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void) {
    return 1;
}

/* ── non-underscore aliases for newlib's reentrant wrappers ─────────────── */
void *sbrk(int incr)                          { return _sbrk(incr); }
int   write(int fd, char *buf, int len)        { return _write(fd, buf, len); }
int   read(int fd, char *buf, int len)         { return _read(fd, buf, len); }
int   open(const char *name, int f, ...)        { return _open(name, f, 0); }
int   close(int fd)                            { return _close(fd); }
int   fstat(int fd, struct stat *st)           { return _fstat(fd, st); }
int   isatty(int fd)                           { return _isatty(fd); }
int   lseek(int fd, int offset, int whence)    { return _lseek(fd, offset, whence); }
int   kill(int pid, int sig)                   { return _kill(pid, sig); }
int   getpid(void)                             { return _getpid(); }
