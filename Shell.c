#include "Shell.h"
#include "mm.h"
#include "kprintf.h"
#include "string.h"
#include "kbd_driver.h"
#include "easyfs.h"
#include "ed.h"

#define LSH_RL_BUFSIZE 1024
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
#define EOF (-1)

extern char *vidptr;
extern unsigned int current_loc;
extern void jump_to_userspace(void);

/* ========================= */
/* Builtins                  */
/* ========================= */

int lsh_help(char **args);
int lsh_ls(char **args);
int lsh_touch(char **args);
int lsh_del(char **args);
int lsh_edit(char **args);
int lsh_clear(char **args);
int lsh_launch(char **args);
int lsh_ring3(char **args);

char *builtin_str[] = {
    "help",
    "ls",
    "touch",
    "del",
    "edit",
    "clear",
    "ring3"
};

int (*builtin_func[]) (char **) = {
    &lsh_help,
    &lsh_ls,
    &lsh_touch,
    &lsh_del,
    &lsh_edit,
    &lsh_clear,
    &lsh_ring3

};

/* ========================= */

void initShell(void) {
    E.screenrows = 24;
    E.screencols = 80;
}

/* ========================= */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    initShell();
    lsh_loop();
    return 0;
}

/* ========================= */

void lsh_loop(void) {
    char *line;
    char **args;
    int status = 1;

    while (status) {
        kprintf("> ");

        line = lsh_read_line();
        if (!line) {
            kprintf("readline failed, halting\n");
            while (1);
        }

        kprintf("\n"); /* newline after user hits enter */

        args = lsh_split_line(line);
        if (!args) {
            mem_free(line);
            continue;
        }

        status = lsh_execute(args);

        mem_free(args);
        mem_free(line);
    }
}

/* ========================= */

char *lsh_read_line(void) {
    int bufsize = LSH_RL_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    if (!buffer) {
        kprintf("allocation error: no buffer\n");
        return NULL;
    }

    while (1) {
        c = kbd_getchar();

        if (c == '\b') {
            if (position > 0) {
                position--;
                /* erase character on screen */
                if (current_loc >= 2) {
                    current_loc -= 2;
                    vidptr[current_loc]     = ' ';
                    vidptr[current_loc + 1] = 0x07;
                }
            }
            continue;
        }

        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            return buffer;
        }

        /* Echo the character */
        vidptr[current_loc++] = (char)c;
        vidptr[current_loc++] = 0x07;

        buffer[position++] = (char)c;

        if (position >= bufsize) {
            bufsize += LSH_RL_BUFSIZE;
            char *newbuf = realloc(buffer, bufsize);
            if (!newbuf) {
                kprintf("allocation error: realloc failed\n");
                mem_free(buffer);
                return NULL;
            }
            buffer = newbuf;
        }
    }
}

/* ========================= */

char **lsh_split_line(char *line) {
    int bufsize = LSH_TOK_BUFSIZE;
    int position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        kprintf("allocation error\n");
        return NULL;
    }

    token = strtok(line, LSH_TOK_DELIM);
    while (token != NULL) {
        tokens[position++] = token;

        if (position >= bufsize) {
            bufsize += LSH_TOK_BUFSIZE;
            char **newtokens = realloc(tokens, bufsize * sizeof(char*));
            if (!newtokens) {
                kprintf("allocation error\n");
                mem_free(tokens);
                return NULL;
            }
            tokens = newtokens;
        }

        token = strtok(NULL, LSH_TOK_DELIM);
    }

    tokens[position] = NULL;
    return tokens;
}

/* ========================= */

int lsh_num_builtins(void) {
    return sizeof(builtin_str) / sizeof(char *);
}

/* ========================= */

int lsh_help(char **args) {
    (void)args;
    kprintf("Custom Kernel Shell\n");
    kprintf("Available commands:\n");
    for (int i = 0; i < lsh_num_builtins(); i++)
        kprintf("  %s\n", builtin_str[i]);
    return 1;
}

int lsh_ls(char **args) {
    (void)args;
    fs_list_files();
    return 1;
}

int lsh_touch(char **args) {
    if (args[1] == NULL) {
        kprintf("touch: no filename given\n");
        return 1;
    }
    fs_create_file(args[1]);
    return 1;
}

int lsh_del(char **args) {
    if (args[1] == NULL) {
        kprintf("del: no filename given\n");
        return 1;
    }
    fs_delete_file(args[1]);
    return 1;
}

int lsh_edit(char **args) {
    if (args[1] == NULL) {
        kprintf("edit: no filename given\n");
        return 1;
    }
    editorRun(args[1]);
    return 1;
}

int lsh_clear(char **args) {
    (void)args;
    clear_screen();
    return 1;
}

int lsh_ring3(char **args) {
    (void)args;
    jump_to_userspace();
    return 1;
}


/* ========================= */

int lsh_execute(char **args) {
    if (args[0] == NULL) return 1;

    for (int i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0)
            return (*builtin_func[i])(args);
    }

    return lsh_launch(args);
}

int lsh_launch(char **args) {
    kprintf("Unknown command: %s\n", args[0]);
    return 1;
}