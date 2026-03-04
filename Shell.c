#include "Shell.h"
#include "mm.h"
#include "kprintf.h"
#include "string.h"
#include "kbd_driver.h"
#include "easyfs.h"

#define LSH_RL_BUFSIZE 1024
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"



#define EOF (-1)

/* ========================= */
/* Builtins                  */
/* ========================= */

int lsh_help(char **args);
int lsh_ls(char **args);
int lsh_touch(char **args);
int lsh_del(char **args);
int lsh_launch(char **args);

char *builtin_str[] = {
    "help",
	"ls",
	"touch",
    "del"
};

int (*builtin_func[]) (char **) = {
    &lsh_help,
	&lsh_ls,
	&lsh_touch
};

/* ========================= */




/* ========================= */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    lsh_loop();
    return 0;
}

/* ========================= */

void lsh_loop(void) {
    char *line;
    char **args;
    int status = 1;   // FIX: must initialize

    while (status) {
        kprintf("> ");

        line = lsh_read_line();
        if (!line) {
            kprintf("readline failed, halting \n");
            while (1);
        }


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
			if(position > 0) position--;
			continue;
		}

        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position++] = (char)c;
        }

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

    for (int i = 0; i < lsh_num_builtins(); i++) {
        kprintf("  %s\n", builtin_str[i]);
    }

    return 1;
}


int lsh_ls(char **args) {
    fs_list_files();
    return 1;
}

int lsh_touch(char **args) {
	fs_create_file(args[1]);
	if(args[1] == NULL) {
	  kprintf("Touch: no file name given\n");
	  return 0;
	}
	return 1;

}


int lsh_del(char **args) {
	fs_delete_file(args[1]);
	if(args[1] == NULL) {
	 kprintf("Del: no file name given\n");
	 return 0;
	}
	return 1;
}


/* ========================= */

int lsh_execute(char **args) {
    if (args[0] == NULL) {
        return 1; // empty command
    }

    for (int i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return lsh_launch(args);
}

/* ========================= */

int lsh_launch(char **args) {
    kprintf("Unknown command: %s\n", args[0]);
    return 1;
}