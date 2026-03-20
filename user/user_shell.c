#include "shed.h"

#define BUFSIZE 1024
#define TOKSIZE 64
#define DELIM " \t\r\n\a"

int my_strlen(const char *s);
int my_strcmp(const char *a, const char *b);
char *my_strchr(const char *s, char c);
char *read_line();
char **split_line(char *line);
void execute(char **args);

__attribute__((section(".text._start"))) void _start(){
    sys_print("\nShed userspace shell\n");
    while (1) {
        sys_print("> ");
        char *line = read_line();
        sys_print("\n");
        char **args = split_line(line);
        execute(args);
        u_free(args);
        u_free(line);
        u_heap_reset();
    }
}




int my_strlen(const char *s) {
    int i = 0;
    while (s[i]) i++;
    return i;
}

int my_strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

char *my_strchr(const char *s, char c) {
    while (*s) { if (*s == c) return (char*)s; s++; }
    return 0;
}

char *read_line() {
    char *buf = &user_heap[heap_pos];
    heap_pos += BUFSIZE;
    int pos = 0;
    char c;
    while (1) {
        c = sys_getchar();
        if (c == '\n') {
            buf[pos] = 0;
            return buf;
        }
        if (c == '\b') {
            if (pos > 0) {
                pos--;
                sys_backspace();
            }
            continue;
        }
        char echo[2] = {c, 0};
        sys_print(echo);
        buf[pos++] = c;
        if (pos >= BUFSIZE - 1) { buf[pos] = 0; return buf; }
    }
}

char **split_line(char *line) {
    char **tokens = u_malloc(TOKSIZE * sizeof(char*));
    int pos = 0;
    char *tok = line;
    char *p = line;

    while (*p) {
        if (my_strchr(DELIM, *p)) {
            if (p > tok) tokens[pos++] = tok;
            *p = 0;
            tok = p + 1;
        }
        p++;
    }
    if (p > tok && *tok) tokens[pos++] = tok;
    tokens[pos] = 0;
    return tokens;
}

void execute(char **args) {
    if (!args[0]) return;

    if (my_strcmp(args[0], "ls") == 0) {
        sys_ls();
    } else if (my_strcmp(args[0], "touch") == 0) {
        if (args[1]) sys_touch(args[1]);
        else sys_print("touch: no filename\n");
    } else if (my_strcmp(args[0], "del") == 0) {
        if (args[1]) sys_del(args[1]);
        else sys_print("del: no filename\n");
    } else if (my_strcmp(args[0], "cat") == 0) {
        if (args[1]) {
            char buf[512];
            sys_read_file(args[1], buf, 512);
            sys_print(buf);
            sys_print("\n");
        } else sys_print("cat: no filename\n");
    } else if (my_strcmp(args[0], "help") == 0) {
        sys_print("commands: ls, touch, del, cat, exit, clear\n");
    } else if (my_strcmp(args[0], "exit") == 0) {
        sys_exit();
    } else if (my_strcmp(args[0], "clear") == 0) {
        sys_clear();
    }
    else {
        sys_print("unknown command\n");
    }
}

