#ifndef ED_H
#define ED_H

#include "kbd_driver.h"  /* for ARROW_* and KEY_* codes */

#define SHED_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)

#define MODE_NORMAL  0
#define MODE_INSERT  1
#define MODE_COMMAND 2

typedef struct erow {
    int size;
    char *chars;
} erow;

typedef struct editorConfig {
    int cx, cy;
    int rowoff, coloff;
    int screenrows, screencols;
    int numrows;
    int dirty;
    int mode;
    erow *rows;
    char filename[64];
    char cmdbuf[80];
    int  cmdlen;
} editorConfig;

extern editorConfig E;

void initEditor(void);
void editorOpen(const char *filename);
void editorSave(void);

void editorInsertRow(int at, const char *s, int len);
void editorFreeRow(erow *row);
void editorDelRow(int at);
void editorRowInsertChar(erow *row, int at, int c);
void editorRowDelChar(erow *row, int at);
void editorRowAppendString(erow *row, const char *s, int len);

void editorInsertChar(int c);
void editorInsertNewline(void);
void editorDelChar(void);
void editorYankLine(void);
void editorPasteLine(void);
void editorDeleteLine(void);

void editorRefreshScreen(void);
void editorDrawRows(void);
void editorDrawStatusBar(void);
void editorScroll(void);

int  editorReadKey(void);
void editorMoveCursor(int key);
void editorProcessKeypress(void);
void editorRun(const char *filename);

#endif /* ED_H */