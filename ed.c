#include "ed.h"
#include "mm.h"
#include "string.h"
#include "kbd_driver.h"
#include "kprintf.h"
#include "easyfs.h"

editorConfig E;

static char yank_buf[BLOCK_SIZE];
static int  yank_len = 0;

/* ============================================================ */
/* Lifecycle                                                     */
/* ============================================================ */

void initEditor(void) {
    E.cx = E.cy = 0;
    E.rowoff = E.coloff = 0;
    E.numrows = 0;
    E.dirty = 0;
    E.mode = MODE_NORMAL;
    E.rows = NULL;
    E.filename[0] = '\0';
    E.cmdbuf[0] = '\0';
    E.cmdlen = 0;
}

void editorOpen(const char *filename) {
    if (!filename) return;
    int i = 0;
    while (filename[i] && i < 63) { E.filename[i] = filename[i]; i++; }
    E.filename[i] = '\0';

    DirEntry *entry = fs_find_file(filename);
    if (!entry || entry->file_size == 0) return;

    char buf[BLOCK_SIZE];
    fs_read_block(entry->first_data_block, buf);
    int len = (int)entry->file_size;
    if (len > BLOCK_SIZE - 1) len = BLOCK_SIZE - 1;
    buf[len] = '\0';

    int start = 0;
    for (int j = 0; j <= len; j++) {
        if (buf[j] == '\n' || buf[j] == '\0') {
            editorInsertRow(E.numrows, &buf[start], j - start);
            start = j + 1;
        }
    }
    E.dirty = 0;
}

void editorSave(void) {
    if (E.filename[0] == '\0') return;
    char buf[BLOCK_SIZE];
    int pos = 0;
    for (int i = 0; i < E.numrows && pos < BLOCK_SIZE - 1; i++) {
        int space = BLOCK_SIZE - 1 - pos;
        int copy  = E.rows[i].size < space ? E.rows[i].size : space;
        memcpy(&buf[pos], E.rows[i].chars, copy);
        pos += copy;
        if (pos < BLOCK_SIZE - 1) buf[pos++] = '\n';
    }
    buf[pos] = '\0';

    if (!fs_find_file(E.filename)) fs_create_file(E.filename);
    DirEntry *entry = fs_find_file(E.filename);
    if (!entry) return;
    fs_write_block(entry->first_data_block, buf);
    entry->file_size = (uint32_t)pos;
    E.dirty = 0;
}

/* ============================================================ */
/* Row operations                                               */
/* ============================================================ */

void editorInsertRow(int at, const char *s, int len) {
    if (at < 0 || at > E.numrows) return;
    E.rows = realloc(E.rows, sizeof(erow) * (E.numrows + 1));
    memmove(&E.rows[at + 1], &E.rows[at], sizeof(erow) * (E.numrows - at));
    E.rows[at].size  = len;
    E.rows[at].chars = malloc(len + 1);
    memcpy(E.rows[at].chars, s, len);
    E.rows[at].chars[len] = '\0';
    E.numrows++;
}

void editorFreeRow(erow *row) {
    mem_free(row->chars);
    row->chars = NULL;
    row->size  = 0;
}

void editorDelRow(int at) {
    if (at < 0 || at >= E.numrows) return;
    editorFreeRow(&E.rows[at]);
    memmove(&E.rows[at], &E.rows[at + 1], sizeof(erow) * (E.numrows - at - 1));
    E.numrows--;
    E.dirty = 1;
}

void editorRowInsertChar(erow *row, int at, int c) {
    if (at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->chars[at] = (char)c;
    row->size++;
    E.dirty = 1;
}

void editorRowDelChar(erow *row, int at) {
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    E.dirty = 1;
}

void editorRowAppendString(erow *row, const char *s, int len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    E.dirty = 1;
}

/* ============================================================ */
/* Editor ops                                                   */
/* ============================================================ */

void editorInsertChar(int c) {
    if (E.cy == E.numrows) editorInsertRow(E.numrows, "", 0);
    editorRowInsertChar(&E.rows[E.cy], E.cx, c);
    E.cx++;
    E.dirty = 1;
}

void editorInsertNewline(void) {
    if (E.cx == 0) {
        editorInsertRow(E.cy, "", 0);
    } else {
        erow *row = &E.rows[E.cy];
        editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.rows[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
    }
    E.cy++;
    E.cx = 0;
}

void editorDelChar(void) {
    if (E.cy == E.numrows) return;
    if (E.cx == 0 && E.cy == 0) return;
    if (E.cx > 0) {
        editorRowDelChar(&E.rows[E.cy], E.cx - 1);
        E.cx--;
        E.dirty = 1;
    } else {
        E.cx = E.rows[E.cy - 1].size;
        editorRowAppendString(&E.rows[E.cy - 1], E.rows[E.cy].chars, E.rows[E.cy].size);
        editorDelRow(E.cy);
        E.cy--;
        E.dirty = 1;
    }
}

void editorYankLine(void) {
    if (E.cy >= E.numrows) return;
    erow *row = &E.rows[E.cy];
    yank_len = row->size < BLOCK_SIZE - 1 ? row->size : BLOCK_SIZE - 1;
    memcpy(yank_buf, row->chars, yank_len);
    yank_buf[yank_len] = '\0';
}

void editorPasteLine(void) {
    if (yank_len == 0) return;
    editorInsertRow(E.cy + 1, yank_buf, yank_len);
    E.cy++;
    E.cx = 0;
}

void editorDeleteLine(void) {
    if (E.numrows == 0) return;
    editorYankLine();
    editorDelRow(E.cy);
    if (E.cy >= E.numrows && E.cy > 0) E.cy--;
    E.cx = 0;
}

/* ============================================================ */
/* Scrolling                                                     */
/* ============================================================ */

void editorScroll(void) {
    if (E.cy < E.rowoff) E.rowoff = E.cy;
    if (E.cy >= E.rowoff + E.screenrows) E.rowoff = E.cy - E.screenrows + 1;
    if (E.cx < E.coloff) E.coloff = E.cx;
    if (E.cx >= E.coloff + E.screencols) E.coloff = E.cx - E.screencols + 1;
}

/* ============================================================ */
/* Rendering                                                     */
/* ============================================================ */

void editorDrawRows(void) {
    for (int y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        kvga_set_cursor(y, 0);

        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenrows / 3) {
                char welcome[80];
                int wlen = ksprintf(welcome,
                    "Shed v%s  --  i=insert  :w=save  :q=quit", SHED_VERSION);
                if (wlen > E.screencols) wlen = E.screencols;
                int pad = (E.screencols - wlen) / 2;
                kvga_write_char(y, 0, '~', 0x07);
                for (int p = 1; p < pad; p++) kvga_write_char(y, p, ' ', 0x07);
                kvga_set_cursor(y, pad);
                kprintf("%.*s", wlen, welcome);
                kvga_clear_to_eol(y, pad + wlen);
            } else {
                kvga_write_char(y, 0, '~', 0x07);
                kvga_clear_to_eol(y, 1);
            }
        } else {
            erow *row  = &E.rows[filerow];
            int   start = E.coloff;
            int   len   = row->size - start;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;
            kprintf("%.*s", len, &row->chars[start]);
            kvga_clear_to_eol(y, len);
        }
    }
}

void editorDrawStatusBar(void) {
    /* Status bar: row E.screenrows */
    const char *mode_str = "NORMAL";
    if (E.mode == MODE_INSERT)  mode_str = "INSERT";
    if (E.mode == MODE_COMMAND) mode_str = "COMMAND";

    char status[80];
    int len = ksprintf(status, " %s | %.20s %s",
        mode_str,
        E.filename[0] ? E.filename : "[No Name]",
        E.dirty > 0 ? "[+]" : "");
    if (len > E.screencols) len = E.screencols;

    for (int i = 0; i < len; i++)
        kvga_write_char(E.screenrows, i, status[i], 0x70);
    for (int i = len; i < E.screencols; i++)
        kvga_write_char(E.screenrows, i, ' ', 0x70);

    /* Command line: row E.screenrows + 1 */
    int cmdrow = E.screenrows + 1;
    kvga_set_cursor(cmdrow, 0);
    if (E.mode == MODE_COMMAND) {
        kprintf(":%.*s", E.cmdlen, E.cmdbuf);
        kvga_clear_to_eol(cmdrow, E.cmdlen + 1);
    } else {
        kvga_clear_to_eol(cmdrow, 0);
    }
}

void editorRefreshScreen(void) {
    editorScroll();
    editorDrawRows();
    editorDrawStatusBar();

    /* Park hardware cursor */
    if (E.mode == MODE_COMMAND) {
        kvga_set_cursor(E.screenrows + 1, E.cmdlen + 1);
    } else {
        kvga_set_cursor(E.cy - E.rowoff, E.cx - E.coloff);
    }
}

/* ============================================================ */
/* Input                                                         */
/* ============================================================ */

/* Dead simple now — the driver hands us cooked key codes */
int editorReadKey(void) {
    return kbd_getchar();
}

void editorMoveCursor(int key) {
    erow *row = (E.cy < E.numrows) ? &E.rows[E.cy] : NULL;
    switch (key) {
        case 'h': case ARROW_LEFT:
            if (E.cx > 0) E.cx--;
            else if (E.cy > 0) { E.cy--; E.cx = E.rows[E.cy].size; }
            break;
        case 'l': case ARROW_RIGHT:
            if (row && E.cx < row->size) E.cx++;
            break;
        case 'k': case ARROW_UP:
            if (E.cy > 0) E.cy--;
            break;
        case 'j': case ARROW_DOWN:
            if (E.cy < E.numrows - 1) E.cy++;
            break;
    }
    row = (E.cy < E.numrows) ? &E.rows[E.cy] : NULL;
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen) E.cx = rowlen;
}

static int editorExecCommand(void) {
    if (strcmp(E.cmdbuf, "q") == 0) {
        if (E.dirty > 0) {
            ksprintf(E.cmdbuf, "No write since last change (use :q!)");
            E.cmdlen = strlen(E.cmdbuf);
            return 0;
        }
        return -1;
    }
    if (strcmp(E.cmdbuf, "q!") == 0) return -1;
    if (strcmp(E.cmdbuf, "w")  == 0) { editorSave(); return 0; }
    if (strcmp(E.cmdbuf, "wq") == 0) { editorSave(); return -1; }

    char tmp[80];
    ksprintf(tmp, "Not an editor command: %s", E.cmdbuf);
    memcpy(E.cmdbuf, tmp, 79);
    E.cmdbuf[79] = '\0';
    E.cmdlen = strlen(E.cmdbuf);
    return 0;
}

void editorProcessKeypress(void) {
    int c = editorReadKey();

    if (E.mode == MODE_INSERT) {
        switch (c) {
            case '\x1b':
                E.mode = MODE_NORMAL;
                if (E.cx > 0) E.cx--;
                break;
            case '\n': editorInsertNewline(); E.dirty = 1; break;
            case '\b': case 127: editorDelChar(); break;
            default:
                if (c >= 32 && c < 127) editorInsertChar(c);
                break;
        }
        return;
    }

    if (E.mode == MODE_COMMAND) {
        switch (c) {
            case '\x1b':
                E.mode = MODE_NORMAL;
                E.cmdbuf[0] = '\0';
                E.cmdlen = 0;
                break;
            case '\n': {
                E.cmdbuf[E.cmdlen] = '\0';
                int result = editorExecCommand();
                if (result == -1) E.dirty = -9999;
                else E.mode = MODE_NORMAL;
                break;
            }
            case '\b': case 127:
                if (E.cmdlen > 0) E.cmdbuf[--E.cmdlen] = '\0';
                break;
            default:
                if (c >= 32 && c < 127 && E.cmdlen < 78) {
                    E.cmdbuf[E.cmdlen++] = (char)c;
                    E.cmdbuf[E.cmdlen]   = '\0';
                }
                break;
        }
        return;
    }

    /* NORMAL mode */
    switch (c) {
        case 'h': case 'j': case 'k': case 'l':
        case ARROW_UP: case ARROW_DOWN:
        case ARROW_LEFT: case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
        case 'i': E.mode = MODE_INSERT; break;
        case 'a':
            E.mode = MODE_INSERT;
            if (E.cy < E.numrows && E.cx < E.rows[E.cy].size) E.cx++;
            break;
        case 'o':
            if (E.cy < E.numrows) E.cx = E.rows[E.cy].size;
            editorInsertNewline();
            E.mode = MODE_INSERT;
            break;
        case 'O':
            E.cx = 0;
            editorInsertRow(E.cy, "", 0);
            E.mode = MODE_INSERT;
            break;
        case 'x':
            if (E.cy < E.numrows && E.cx < E.rows[E.cy].size) {
                editorRowDelChar(&E.rows[E.cy], E.cx);
                if (E.cx >= E.rows[E.cy].size && E.cx > 0) E.cx--;
            }
            break;
        case 'd': {
            int next = editorReadKey();
            if (next == 'd') editorDeleteLine();
            break;
        }
        case 'y': {
            int next = editorReadKey();
            if (next == 'y') editorYankLine();
            break;
        }
        case 'p': editorPasteLine(); break;
        case ':':
            E.mode = MODE_COMMAND;
            E.cmdbuf[0] = '\0';
            E.cmdlen = 0;
            break;
        default: break;
    }
}

/* ============================================================ */
/* Main loop                                                     */
/* ============================================================ */

void editorRun(const char *filename) {
    initEditor();
    E.screenrows = LINES - 2; /* text area | status bar | command line */
    E.screencols = COLUMNS_IN_LINE;
    editorOpen(filename);

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
        if (E.dirty == -9999) {
            clear_screen();
            break;
        }
    }
}