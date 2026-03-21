/*
 * user_shell.c - Shed userspace shell
 * Now with actual libc like civilised people
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shed.h"

#define BUFSIZE 1024
#define TOKSIZE 64
#define DELIM   " \t\r\n\a"

/* forward declarations */
char  *read_line(void);
char **split_line(char *line);
void   execute(char **args);

__attribute__((section(".text._start"))) void _start(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);
    printf("\nShed userspace shell\n");
    while (1) {
        printf("> ");
        char *line = read_line();
        printf("\n");
        char **args = split_line(line);
        execute(args);
        free(args);
        free(line);
    }
}

char *read_line(void) {
    char *buf = malloc(BUFSIZE);
    int pos = 0;
    char c;
    while (1) {
        c = sys_getchar();
        if (c == '\n') { buf[pos] = '\0'; return buf; }
        if (c == '\b') {
            if (pos > 0) { pos--; sys_backspace(); }
            continue;
        }
        putchar(c);
        buf[pos++] = c;
        if (pos >= BUFSIZE - 1) { buf[pos] = '\0'; return buf; }
    }
}

char **split_line(char *line) {
    char **tokens = malloc(TOKSIZE * sizeof(char *));
    int pos = 0;
    char *tok = strtok(line, DELIM);
    while (tok) {
        tokens[pos++] = tok;
        tok = strtok(NULL, DELIM);
    }
    tokens[pos] = NULL;
    return tokens;
}

void execute(char **args) {
    if (!args[0]) return;

    if (strcmp(args[0], "ls") == 0) {
        sys_ls();
    } else if (strcmp(args[0], "touch") == 0) {
        if (args[1]) sys_touch(args[1]);
        else printf("touch: no filename\n");
    } else if (strcmp(args[0], "del") == 0) {
        if (args[1]) sys_del(args[1]);
        else printf("del: no filename\n");
    } else if (strcmp(args[0], "cat") == 0) {
        if (args[1]) {
            char *buf = malloc(512);
            sys_read_file(args[1], buf, 512);
            printf("%s\n", buf);
            free(buf);
        } else printf("cat: no filename\n");
    } else if (strcmp(args[0], "edit") == 0) {
        if (args[1]) sys_exec_editor(args[1]);
        else printf("edit: no filename\n");
    } else if (strcmp(args[0], "help") == 0) {
        printf("commands: ls, touch, del, cat, edit, clear, exit\n");
    } else if (strcmp(args[0], "clear") == 0) {
        sys_clear();
    } else if (strcmp(args[0], "exit") == 0) {
        sys_exit();
    } else {
        printf("unknown command: %s\n", args[0]);
    }
}
