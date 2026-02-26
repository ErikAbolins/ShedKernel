#ifndef SHELL_H
#define SHELL_H

/* Main loop */
void lsh_loop(void);

/* Input handling */
char *lsh_read_line(void);
char **lsh_split_line(char *line);

/* Execution */
int lsh_execute(char **args);
int lsh_launch(char **args);

/* Builtins */
int lsh_help(char **args);
int lsh_num_builtins(void);

#endif /* SHELL_H */