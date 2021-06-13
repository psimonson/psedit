/*
 * main.c - Basic text editor source code.
 *
 * Author: Philip R. Simonson
 * Date  : 06/06/2021
 *
 ****************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ncurses.h>

/* ---------------------------- Editor Stuff ------------------------- */

typedef struct editor {
    int cx, cy;
    int rows, cols;
    int skiplines;
    int linecount;
    char *data;
    long size;
} editor_t;

/* Initialise the editor structure.
 */
editor_t editor_init(void)
{
    editor_t e;
    e.cx = 0;
    e.cy = 0;
    e.rows = 0;
    e.cols = 0;
    e.skiplines = 0;
    e.linecount = 0;
    e.data = NULL;
    e.size = 0;
    return e;
}
/* Destroy editor data.
 */
void editor_free(editor_t *e)
{
    free(e->data);
}
/* Get line from given offset in file.
 */
long editor_getline(editor_t *e, long offset)
{
    long i, nlines = 0;

    for(i = 0; i < e->size && i != offset; i++) {
        if(e->data[i] == '\n' || e->data[i] == 0)
            nlines++;
    }
    return nlines;
}
/* Get offset of given line in file.
 */
long editor_getoffset(editor_t *e, int line_num)
{
    long i, nlines = 0;

    for(i = 0; i < e->size && line_num != nlines; i++) {
        if(e->data[i] == '\n' || e->data[i] == 0)
            nlines++;
    }
    return i;
}
/* Get total number of lines in file.
 */
long editor_getlinecount(editor_t *e)
{
    long i, nlines = 0;
    for(i = 0; i < e->size; i++) {
        if(e->data[i] == '\n' || e->data[i] == 0)
            nlines++;
    }
    return nlines;
}
/* Open a file with the editor.
 */
int editor_open(editor_t *e, const char *filename)
{
    long total;
    FILE *fp;

    // Try to open file.
    if((fp = fopen(filename, "rb")) == NULL)
        return 1;

    // File is open so get length.
    fseek(fp, 0, SEEK_END);
    e->size = ftell(fp);
    rewind(fp);

    // Now allocate buffer and fill it.
    e->data = malloc(sizeof(char) * (e->size + 1));
    if(e->data == NULL) {
        fclose(fp);
        return 2;
    }
    total = fread(e->data, sizeof(char), e->size, fp);
    fclose(fp);
    e->data[e->size] = 0;
    return (total != e->size);
}
/* Save a file from the editor (also creating a backup).
 */
int editor_save(editor_t *e, const char *filename)
{
    char fname[512];
    long total, size;
    char *buf;
    FILE *fp;

    // Open existing file and create backup.
    if((fp = fopen(filename, "rb")) != NULL) {
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        rewind(fp);
        snprintf(fname, sizeof(fname), "%s.bak", filename);
        buf = malloc(sizeof(char) * (size + 1));
        if(buf == NULL) {
            fclose(fp);
            return 1;
        }
        total = fread(buf, sizeof(char), size, fp);
        fclose(fp);
        if(total != size) {
            free(buf);
            return 2;
        }
        if((fp = fopen(fname, "wb")) == NULL) {
            free(buf);
            return 3;
        }
        total = fwrite(buf, sizeof(char), size, fp);
        fclose(fp);
        free(buf);
        if(total != size)
            return 4;
        // Backup created successfully.
    } else {
        // No original file or cannot be opened.
    }

    // Now finally save the new file.
    if((fp = fopen(filename, "wb")) == NULL)
        return 5;
    total = fwrite(e->data, sizeof(char), e->size, fp);
    fclose(fp);
    if(total != e->size)
        return 6;
    // File written successfully.
    return 0;
}
/* Create a blank buffer for editor (new file).
 */
int editor_create(editor_t *e)
{
    // Free and initialise editor.
    editor_free(e);
    *e = editor_init();

    // Create new buffer.
    e->data = calloc(e->size + 1, sizeof(char));
    if(e->data == NULL) {
        return 1;
    }
    return 0;
}
/* Delete a character from the editor buffer.
 */
void editor_delchr(editor_t *e, long at)
{
    if(at < 0 || at > e->size) return;
    memmove(&e->data[at], &e->data[at + 1], e->size-at);
    e->size--;
}
/* Insert a character into the editor buffer.
 */
void editor_inschr(editor_t *e, long at, char ch)
{
    if(at < 0 || at > e->size) at = e->size;
    e->data = realloc(e->data, sizeof(char) * (e->size + 2));
    memmove(&e->data[at + 1], &e->data[at], e->size-at+1);
    e->data[at] = ch;
    e->size++;
}
/* Render text buffer from editor.
 */
void editor_render(editor_t *e)
{
    int x, y;

    for(y = 0; y < e->rows; y++) {
        unsigned long startx = editor_getoffset(e, y + e->skiplines);
        unsigned long endx = editor_getoffset(e, (y + e->skiplines) + 1);
        int size = endx - startx;
        size = (size > e->cols ? e->cols : size);
        for(x = 0; x < size; x++) {
            mvaddch(y, x, e->data[startx + x]);
        }
    }
}

/* ---------------------------- Main Functions ------------------------- */

#define MAXTABSTOP 4
#define CTRL_KEY(x) ((x) & 0x1F)
#define KEY_RETURN 0x0A
#define KEY_TABSTOP 0x09
#define KEY_BACKSPC 127

/* Initialise ncurses library.
 */
static void ncurses_init(void)
{
    initscr();
    cbreak();
    noecho();
    raw();
    keypad(stdscr, TRUE);
    atexit((void (*)(void))endwin);
}
/* Entry point for text editor.
 */
int main(int argc, char *argv[])
{
    unsigned long offset;
    long curline;
    editor_t e;
    int rc, c;

    // Take a filename as an argument.
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    // Initialise editor.
    e = editor_init();
    if(editor_open(&e, argv[1]) != 0) {
        fprintf(stderr, "Warning: Could not open file, creating...\n");
        if(editor_create(&e) != 0) {
            fprintf(stderr, "Error: New buffer cannot be created.\n");
            return 1;
        }
    }
    ncurses_init();
    getmaxyx(stdscr, e.rows, e.cols);
    clear();
    editor_render(&e);
    refresh();
    move(e.cy, e.cx);

    while((c = getch()) != CTRL_KEY('q')) {
        long startx = 0;
        long endx = 0;

        // Handle keyboard input.
        switch(c) {
            case CTRL_KEY('s'):
                if(editor_save(&e, argv[1]) != 0) {
                    // TODO: Display error message.
                }
            break;
            case KEY_UP:
                e.linecount = editor_getlinecount(&e);
                if(e.cy != 0) {
                    e.cy--;
                    startx = editor_getoffset(&e, e.cy + e.skiplines);
                    endx = editor_getoffset(&e, (e.cy + e.skiplines) + 1);
                    if(e.cx > (endx - startx) - 1)
                        e.cx = (endx - startx) == 0 ? 0 : (endx - startx) - 1;
                }
                else {
                    if(e.skiplines > 0)
                        e.skiplines--;
                    else
                        e.skiplines = 0;
                }
            break;
            case KEY_DOWN:
                e.linecount = editor_getlinecount(&e);
                if(e.cy != e.rows && (e.cy + e.skiplines) != e.linecount) {
                    e.cy++;
                    startx = editor_getoffset(&e, e.cy + e.skiplines);
                    endx = editor_getoffset(&e, (e.cy + e.skiplines) + 1);
                    if(e.cx > (endx - startx) - 1)
                        e.cx = (endx - startx) == 0 ? 0 : (endx - startx) - 1;
                }
                else if(e.cy >= e.rows) {
                    int skiptotal = e.linecount - e.rows;
                    if(e.skiplines < skiptotal)
                        e.skiplines++;
                    else
                        e.skiplines = skiptotal;
                }
            break;
            case KEY_LEFT:
                if(e.cx != 0) {
                    e.cx--;
                }
            break;
            case KEY_RIGHT:
                startx = editor_getoffset(&e, e.cy + e.skiplines);
                endx = editor_getoffset(&e, (e.cy + e.skiplines) + 1);
                if(e.cx != (endx - startx) - 1) {
                    e.cx++;
                }
            break;
            case KEY_HOME:
                if(e.cx != 0) {
                    e.cx = 0;
                }
            break;
            case KEY_END:
                startx = editor_getoffset(&e, e.cy + e.skiplines);
                endx = editor_getoffset(&e, (e.cy + e.skiplines) + 1);
                if(e.cx != (endx - startx) - 1) {
                    int len = (endx - startx);
                    e.cx = len > 0 ? len - 1 : len;
                }
            break;
            case KEY_DC:
                startx = editor_getoffset(&e, e.cy + e.skiplines);
                endx = editor_getoffset(&e, (e.cy + e.skiplines) + 1);
                if(e.cx >= 0 && e.cx < (endx - startx)) {
                    editor_delchr(&e, startx + e.cx);
                }
            break;
            case KEY_BACKSPC:
                startx = editor_getoffset(&e, e.cy + e.skiplines);
                endx = editor_getoffset(&e, (e.cy + e.skiplines) + 1);
                if(e.cx > 0 && e.cx <= (endx - startx)) {
                    e.cx--;
                    move(e.cy, e.cx);
                    editor_delchr(&e, startx + e.cx);
                } else if(e.cx == 0 && e.cy > 0) {
                    startx = editor_getoffset(&e, (e.cy - 1) + e.skiplines);
                    endx = editor_getoffset(&e, e.cy + e.skiplines);
                    e.cx = (endx - startx) - 1;
                    e.cy--;
                    move(e.cy, e.cx);
                    editor_delchr(&e, startx + e.cx);
                }
            break;
            case KEY_TABSTOP:
                if(e.cy < e.cols) {
                    int tabstop = MAXTABSTOP;
                    startx = editor_getoffset(&e, e.cy + e.skiplines);
                    while(tabstop-- > 0) {
                        editor_inschr(&e, startx + e.cx, '\t');
                        e.cx++;
                    }
                }
            break;
            case KEY_ENTER:
            case KEY_RETURN:
                e.linecount = editor_getlinecount(&e);
                startx = editor_getoffset(&e, e.cy + e.skiplines);
                editor_inschr(&e, startx + e.cx, '\n');
                if(e.cy < e.rows)
                    e.cy++;
                else {
                    int skiptotal = (e.linecount - e.rows) - 1;
                    if(e.skiplines <= skiptotal)
                        e.skiplines++;
                    else
                        e.skiplines = skiptotal + 1;
                }
                e.cx = 0;
            break;
            default:
                if(isprint(c)) {
                    if(e.cy <= e.cols) {
                        startx = editor_getoffset(&e, e.cy + e.skiplines);
                        editor_inschr(&e, startx + e.cx, c);
                        e.cx++;
                    }
                }
            break;
        }
        clear();
        editor_render(&e);
        move(e.cy, e.cx);
        refresh();
    }

    editor_free(&e);
    return 0;
}
