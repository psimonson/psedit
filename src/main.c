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

#define EDITOR_PAIR 1
#define STATUS_PAIR 2

#define MAXSKIPROW 20

typedef struct editor {
    int cx, cy;
    int rows, cols;
    long skipcols;
    long skiprows;
    long linecount;
    bool dirty;
    char *data;
    long unsigned size;
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
    e.skipcols = 0;
    e.skiprows = 0;
    e.linecount = 0;
    e.dirty = true;
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
long unsigned editor_getline(editor_t *e, long unsigned offset)
{
    long unsigned i, nlines = 0;

    for(i = 0; i < e->size && i != offset; i++) {
        if(e->data[i] == '\n')
            nlines++;
    }
    return nlines;
}
/* Get offset of given line in file.
 */
long unsigned editor_getoffset(editor_t *e, long line_num)
{
    long nlines = 0;
    long unsigned i;

    for(i = 0; i < e->size && line_num != nlines; i++) {
        if(e->data[i] == '\n')
            nlines++;
    }
    return i;
}
/* Get total number of lines in file.
 */
void editor_getlinecount(editor_t *e)
{
    long unsigned i, nlines = 0;
    for(i = 0; i < e->size; i++) {
        if(e->data[i] == '\n')
            nlines++;
    }
    e->linecount = nlines;
}
/* Search through a file with the editor.
 */
void editor_find(editor_t *e, const char *string)
{
    char *p;

    // Search through the file.
    p = strstr(e->data, string);
    if(p != NULL) {
        long unsigned offset = p - e->data;
        long lines = editor_getline(e, offset);
        long unsigned offset2;

        // Calculate cursor position in buffer.
        if(lines > e->rows) {
            e->skiprows = lines - (e->rows - 2);
        }
        else {
            e->skiprows = 0;
        }
        e->cy = lines - e->skiprows;
        offset2 = editor_getoffset(e, e->cy + e->skiprows);
        e->cx = offset - offset2;
    }
}
/* Open a file with the editor.
 */
int editor_open(editor_t *e, const char *filename)
{
    long unsigned total;
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
    if(total != e->size) {
        fprintf(stderr, "Error: Cannot open file, size doesn't match.\n");
        free(e->data);
        return (total != e->size);
    }
    e->data[e->size] = 0;
    return 0;
}
/* Save a file from the editor (also creating a backup).
 */
int editor_save(editor_t *e, const char *filename)
{
    char fname[512];
    long unsigned total, size;
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
void editor_delchr(editor_t *e, long unsigned at)
{
    if(at > e->size) return;
    memmove(&e->data[at], &e->data[at + 1], e->size-at);
    e->size--;
    editor_getlinecount(e);
}
/* Insert a character into the editor buffer.
 */
static void _editor_inschr(editor_t *e, long unsigned at, char ch)
{
    if(at > e->size) at = e->size;
    e->data = realloc(e->data, sizeof(char) * (e->size + 2));
    memmove(&e->data[at + 1], &e->data[at], e->size-at+1);
    e->data[at] = ch;
    e->size++;
}
/* Insert a character into the editor buffer with automatic new line.
 */
void editor_inschr(editor_t *e, long unsigned at, char ch)
{
    if(e->linecount == 0) {
        _editor_inschr(e, at, '\n');
    }
    _editor_inschr(e, at, ch);
    editor_getlinecount(e);
}
/* Clear a line on the screen.
 */
void editor_clearline(editor_t *e, int line)
{
    register int i;

    if(line < 0 || line > e->rows - 1) return;

    for(i = 0; i < e->cols; i++) {
        mvaddch(line, i, ' ');
    }
}
/* Render text buffer from editor.
 */
void editor_render(editor_t *e)
{
    register int x, y;
    long unsigned startx, endx;
    long size;

    // Display the text on the screen from the editor buffer.
    for(y = 0; y < e->rows - 1; y++) {
        startx = editor_getoffset(e, y + e->skiprows);
        endx = editor_getoffset(e, (y + e->skiprows) + 1);
        size = ((endx - startx) > 0 ? (endx - startx) : 0);

        // Render line of text to screen.
        for(x = 0; x < size; x++) {
            if(has_colors())
                attron(COLOR_PAIR(EDITOR_PAIR));
            mvaddch(y, x - e->skipcols, e->data[startx + x]);
            if(has_colors())
                attron(COLOR_PAIR(EDITOR_PAIR));
        }

        // Fill the rest of the line with blanks.
        for(x = (size - 1) - e->skipcols; x < e->cols; x++) {
            if(has_colors())
                attron(COLOR_PAIR(EDITOR_PAIR));
            mvaddch(y, x, ' ');
            if(has_colors())
                attroff(COLOR_PAIR(EDITOR_PAIR));
        }
    }

    // Fill status bar.
    if(has_colors())
        attron(COLOR_PAIR(STATUS_PAIR));
    editor_clearline(e, e->rows - 1);
    if(has_colors())
        attron(COLOR_PAIR(STATUS_PAIR));
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

    // Initialize colors
    if(has_colors()) {
        start_color();
        init_pair(EDITOR_PAIR, COLOR_RED, COLOR_WHITE);
        init_pair(STATUS_PAIR, COLOR_WHITE, COLOR_RED);
    }
}
/* Entry point for text editor.
 */
int main(int argc, char *argv[])
{
    long unsigned offset;
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
    editor_getlinecount(&e);
    ncurses_init();
    getmaxyx(stdscr, e.rows, e.cols);
    clear();
    editor_render(&e);
    refresh();
    move(e.cy, e.cx);

    while((c = getch()) != CTRL_KEY('q')) {
        long startx = 0;
        long endx = 0;

        // Get line count of editor.
        editor_getlinecount(&e);

        // Handle keyboard input.
        switch(c) {
            case CTRL_KEY('s'): {
                char status[80];
                int i, len;

                if(has_colors())
                    attron(COLOR_PAIR(STATUS_PAIR));

                if(editor_save(&e, argv[1]) != 0) {
                    len = snprintf(status, sizeof(status),
                        "Error: Saving file %s.", argv[1]);
                }
                else {
                    len = snprintf(status, sizeof(status),
                        "Saving file %s totaling %ld bytes.",
                        argv[1], e.size - 1);
                }

                // Draw message to status bar.
                mvprintw(e.rows - 1, 0, "%s", status);

                // Fill rest of buffer with STATUS_PAIR color.
                for(i = len; i < e.cols; i++) {
                    if(has_colors())
                        attron(COLOR_PAIR(STATUS_PAIR));
                    mvaddch(e.rows - 1, i, ' ');
                    if(has_colors())
                        attroff(COLOR_PAIR(STATUS_PAIR));
                }

                if(has_colors())
                    attroff(COLOR_PAIR(STATUS_PAIR));
                refresh();
            } break;
            case CTRL_KEY('f'):
                // Find in file.
                editor_find(&e, "find");
                e.dirty = true;
            break;
            case KEY_UP:
                if(e.cy != 0) {
                    e.cy--;
                }
                else {
                    if(e.skiprows > 0)
                        e.skiprows--;
                    else
                        e.skiprows = 0;
                    e.dirty = true;
                }

                // Get start and end of current line.
                startx = editor_getoffset(&e, e.cy + e.skiprows);
                endx = editor_getoffset(&e, (e.cy + e.skiprows) + 1);

                // Reset cursor position to snap to end of line.
                if((e.cx + e.skipcols) > (endx - startx) - 1) {
                    int len = (endx - startx) - 1;
                    int skipcols = len - e.cols + 1;
                    int skipcx = (len - skipcols) % e.cols;

                    // Calculate how many columns to skip.
                    e.skipcols = (len >= e.cols ? skipcols: 0);
                    // Calculate how many columns left from length.
                    e.cx = len >= e.cols ? skipcx : len > 0 ? len : 0;
                    e.dirty = true;
                }
            break;
            case KEY_DOWN:
                if(e.cy != (e.rows - 2) &&
                    (e.cy + e.skiprows) < (e.linecount - 1)) {
                    e.cy++;
                }
                else if(e.cy >= (e.rows - 2) &&
                    (e.cy + e.skiprows) < (e.linecount - 1)) {
                    int skiptotal = e.linecount - (e.rows - 2);
                    if(e.skiprows < skiptotal)
                        e.skiprows++;
                    else
                        e.skiprows = skiptotal;
                    e.dirty = true;
                }

                // Get start and end of current line.
                startx = editor_getoffset(&e, e.cy + e.skiprows);
                endx = editor_getoffset(&e, (e.cy + e.skiprows) + 1);

                // Reset cursor position to snap to end of line.
                if((e.cx + e.skipcols) > (endx - startx) - 1) {
                    int len = (endx - startx) - 1;
                    int skipcols = len - e.cols + 1;
                    int skipcx = (len - skipcols) % e.cols;

                    // Calculate how many columns to skip.
                    e.skipcols = (len >= e.cols ? skipcols: 0);
                    // Calculate how many columns left from length.
                    e.cx = len >= e.cols ? skipcx : len > 0 ? len : 0;
                    e.dirty = true;
                }
            break;
            case KEY_LEFT:
                if(e.cx != 0) {
                    e.cx--;
                }
                else {
                    if(e.skipcols > 0)
                        e.skipcols--;
                    else
                        e.skipcols = 0;
                    e.dirty = true;
                }
            break;
            case KEY_RIGHT:
                startx = editor_getoffset(&e, e.cy + e.skiprows);
                endx = editor_getoffset(&e, (e.cy + e.skiprows) + 1);
                if(e.cx < (e.cols - 1) && e.cx < (endx - startx) - 1) {
                    e.cx++;
                }
                else if(e.cx >= (e.cols - 1) &&
                    (e.cx + e.skipcols) < (endx - startx) - 1) {
                    int len = endx - startx;
                    int skiptotal = len >= e.cols ? len - e.cols + 1 : 0;
                    if(e.skipcols < skiptotal)
                        e.skipcols++;
                    else
                        e.skipcols = skiptotal;
                    e.dirty = true;
                }
            break;
            case KEY_PPAGE:
                if(e.skiprows > MAXSKIPROW)
                    e.skiprows -= MAXSKIPROW;
                else
                    e.skiprows = 0;

                startx = editor_getoffset(&e, e.cy + e.skiprows);
                endx = editor_getoffset(&e, (e.cy + e.skiprows) + 1);

                // Snap cursor to end of line.
                if((e.cx + e.skipcols) > (endx - startx) - 1) {
                    int len = (endx - startx) - 1;
                    int skipcols = len - e.cols + 1;
                    int skipcx = (len - skipcols) % e.cols;

                    e.skipcols = len >= e.cols ? skipcols : 0;
                    e.cx = len >= e.cols ? skipcx : len > 0 ? len : 0;
                }
                e.dirty = true;
            break;
            case KEY_NPAGE:
                if(e.linecount > MAXSKIPROW) {
                    if(e.skiprows < (e.linecount - e.rows + 1) - MAXSKIPROW)
                        e.skiprows += MAXSKIPROW;
                    else
                        e.skiprows = e.linecount - e.rows + 1;
                }

                startx = editor_getoffset(&e, e.cy + e.skiprows);
                endx = editor_getoffset(&e, (e.cy + e.skiprows) + 1);

                // Snap cursor to end of line.
                if((e.cx + e.skipcols) > (endx - startx) - 1) {
                    int len = (endx - startx) - 1;
                    int skipcols = len - e.cols + 1;
                    int skipcx = (len - skipcols) % e.cols;

                    e.skipcols = len >= e.cols ? skipcols : 0;
                    e.cx = len >= e.cols ? skipcx : len > 0 ? len : 0;
                }
                e.dirty = true;
            break;
            case KEY_HOME:
                if((e.cx + e.skipcols) != 0) {
                    e.cx = 0;
                    e.skipcols = 0;
                    e.dirty = true;
                }
            break;
            case KEY_END:
                startx = editor_getoffset(&e, e.cy + e.skiprows);
                endx = editor_getoffset(&e, (e.cy + e.skiprows) + 1);
                if((e.cx + e.skipcols) < (e.cols - 1) &&
                    (e.cx + e.skipcols) < (endx - startx)) {
                    int len = (endx - startx) - 1;
                    int skipcols = len - e.cols + 1;
                    int skipcx = (len - skipcols) % e.cols;

                    // Calculate how many columns to skip.
                    e.skipcols = (len >= e.cols ? skipcols: 0);
                    // Calculate how many columns left from length.
                    e.cx = len >= e.cols ? skipcx : len > 0 ? len : 0;
                    e.dirty = true;
                }
            break;
            case KEY_DC:
                startx = editor_getoffset(&e, e.cy + e.skiprows);
                endx = editor_getoffset(&e, (e.cy + e.skiprows) + 1);
                if((e.cx + e.skipcols) >= 0 &&
                    (e.cx + e.skipcols) <= (endx - startx) &&
                    (e.cy + e.skiprows) < e.linecount) {
                    editor_delchr(&e, startx + (e.cx + e.skipcols));
                }
                e.dirty = true;
            break;
            case KEY_BACKSPC:
                startx = editor_getoffset(&e, e.cy + e.skiprows);
                endx = editor_getoffset(&e, (e.cy + e.skiprows) + 1);
                if((e.cx + e.skipcols) > 0 &&
                    (e.cx + e.skipcols) <= (endx - startx) &&
                    (e.cy + e.skiprows) < e.linecount) {
                    if(e.cx > 0)
                        e.cx--;
                    else
                        e.skipcols--;
                    editor_delchr(&e, startx + (e.cx + e.skipcols));
                }
                else if((e.cx + e.skipcols) == 0 && (e.cy + e.skiprows) > 0 &&
                    (e.cy + e.skiprows) < e.linecount) {
                    int len;

                    startx = editor_getoffset(&e, (e.cy - 1) + e.skiprows);
                    endx = editor_getoffset(&e, e.cy + e.skiprows);
                    len = (endx - startx);
                    if(len >= (e.cols - 1)) {
                        int skipcols = len - e.cols + 1;
                        int skipcx = (len - skipcols) % e.cols;

                        // Calculate how many columns to skip.
                        e.skipcols = (len >= e.cols ? skipcols: 0);
                        // Calculate how many columns left from length.
                        e.cx = len >= e.cols ? skipcx : len > 0 ? len : 0;
                    } else {
                        e.skipcols = 0;
                        e.cx = len > 0 ? len - 1 : 0;
                    }
                    e.cy--;
                    editor_delchr(&e, startx + (e.cx + e.skipcols));
                }
                else if((e.cx + e.skipcols) == 0 && (e.cy + e.skiprows) == 0 &&
                    (e.cy + e.skiprows) < e.linecount) {
                    if(e.skiprows > 0) {
                        int len, skipcols;

                        e.skiprows--;
                        startx = editor_getoffset(&e, e.cy + e.skiprows);
                        endx = editor_getoffset(&e, (e.cy + e.skiprows) + 1);
                        len = (endx - startx);
                        if(len >= (e.cols - 1)) {
                            int skipcols = len - e.cols + 1;
                            int skipcx = (len - skipcols) % e.cols;

                            // Calculate how many columns to skip.
                            e.skipcols = (len >= e.cols ? skipcols: 0);
                            // Calculate how many columns left from length.
                            e.cx = len >= e.cols ? skipcx : len > 0 ? len : 0;
                        } else {
                            e.skipcols = 0;
                            e.cx = len > 0 ? len - 1 : 0;
                        }
                        editor_delchr(&e, startx + (e.cx + e.skipcols));
                    }
                    else {
                        e.skiprows = 0;
                    }
                }
                e.dirty = true;
            break;
            case KEY_TABSTOP: {
                int tabstop = MAXTABSTOP;
                startx = editor_getoffset(&e, e.cy + e.skiprows);
                while(tabstop-- > 0) {
                    editor_inschr(&e, startx + (e.cx + e.skipcols), ' ');
                    if(e.cx < (e.cols - 1))
                        e.cx++;
                    else
                        e.skipcols++;
                }
                e.dirty = true;
            } break;
            case KEY_ENTER:
            case KEY_RETURN:
                startx = editor_getoffset(&e, e.cy + e.skiprows);
                editor_inschr(&e, startx + (e.cx + e.skipcols), '\n');
                if(e.cy != (e.rows - 2))
                    e.cy++;
                else {
                    int skiptotal = e.linecount - (e.rows - 1);
                    if(e.skiprows < skiptotal)
                        e.skiprows++;
                    else
                        e.skiprows = skiptotal;
                }
                e.cx = 0;
                e.skipcols = 0;
                e.dirty = true;
            break;
            default:
                if(isprint(c)) {
                    if(e.cx < e.cols && e.cy < (e.rows - 1)) {
                        startx = editor_getoffset(&e, e.cy + e.skiprows);
                        editor_inschr(&e, startx + (e.cx + e.skipcols), c);
                        if(e.cx < (e.cols - 1))
                            e.cx++;
                        else
                            e.skipcols++;
                        e.dirty = true;
                    }
                }
            break;
        }

        // Clear screen and repaint text.
        if(e.dirty) {
            switch(c) {
                case KEY_LEFT:
                case KEY_RIGHT:
                case KEY_HOME:
                case KEY_END:
                case KEY_PPAGE:
                case KEY_NPAGE:
                    clear();
                break;
                case KEY_UP:
                case KEY_DOWN:
                    editor_clearline(&e, e.cy);
                break;
            }
            editor_render(&e);
        }

        // Move cursor and refresh screen.
        move(e.cy, e.cx);
        if(e.dirty) {
            refresh();
            e.dirty = false;
        }
    }

    editor_free(&e);
    return 0;
}
