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
#define MAXTABSTOP 4

typedef struct editor {
    int cx, cy;
    int rows, cols;
    long skipcols;
    long skiprows;
    long linecount;
    bool dirty;
    bool status_on;
    char status[80];
    char *data;
    char *findstr;
    long unsigned find;
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
    e.find = 0;
    e.findstr = NULL;
    e.status_on = false;
    e.dirty = true;
    e.data = NULL;
    e.size = 0;
    memset(e.status, 0, sizeof(e.status));
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
/* Convert CR/LF in to LF.
 */
void editor_convnewline(editor_t *e)
{
    extern void editor_delchr(editor_t *e, long unsigned at);
    long unsigned i;

    for(i = 0; i < e->size; i++) {
        if(e->data[i] == '\r')
            editor_delchr(e, i);
    }
}
/* Convert tabs to spaces and back again.
 */
void editor_convtab(editor_t *e, bool totab)
{
    extern void editor_inschr(editor_t *e, long unsigned at, char ch);
    extern void editor_delchr(editor_t *e, long unsigned at);
    long unsigned i;

    for(i = 0; i < e->size; i++) {
        if(e->size > 2) {
            if(totab) { // Convert to tabs.
                if(e->data[i] == '\n' && e->data[i + 1] == ' ') {
                    short unsigned spaces = 0;
                    while(e->data[i + 1] == ' ') {
                        ++spaces;
                        editor_delchr(e, i + 1);
                        if(spaces == MAXTABSTOP) {
                            spaces = 0;
                            editor_inschr(e, i + 1, '\t');
                            ++i;
                        }
                    }
                }
            }
            else { // Convert to spaces.
                if(e->data[i] == '\n' && e->data[i + 1] == '\t') {
                    while(e->data[i + 1] == '\t') {
                        short unsigned spaces = 0;
                        editor_delchr(e, i + 1);
                        while(spaces < MAXTABSTOP) {
                            editor_inschr(e, i + 1, ' ');
                            ++spaces;
                        }
                        i += spaces;
                    }
                }
            }
        }
    }
}
/* Get query string for searching.
 */
char *editor_findprompt(editor_t *e, const char *string)
{
    void editor_setstatus(editor_t *e, const char *fmt, ...);
    void editor_renderstatus(editor_t *e);
    static char query[80];
    int c, i, cx, cy, newx, newy;

    // Save original cx and cy
    cx = e->cx;
    cy = e->cy;
    editor_setstatus(e, "Find: ");
    if(has_colors())
        attron(COLOR_PAIR(STATUS_PAIR));
    editor_renderstatus(e);
    if(has_colors())
        attroff(COLOR_PAIR(STATUS_PAIR));
    getyx(stdscr, newy, newx);

    // Get string
    for(i = 0; i < 80 && (c = getch()) != '\n'; ) {
        if(c == '\x1b') {
            return NULL;
        }
        else if(c == KEY_BACKSPACE || c == 127) {
            if(i > 0) {
                query[--i] = '\0';
                if(has_colors())
                    attron(COLOR_PAIR(STATUS_PAIR));
                mvaddch(newy, --newx, ' ');
                if(has_colors())
                    attroff(COLOR_PAIR(STATUS_PAIR));
                move(newy, newx);
            }
        }
        else {
            if(i < 80 && i < (e->cols - 7) && isprint(c)) {
                query[i++] = c;
                if(has_colors())
                    attron(COLOR_PAIR(STATUS_PAIR));
                mvaddch(newy, newx++, c);
                if(has_colors())
                    attroff(COLOR_PAIR(STATUS_PAIR));
            }
        }
    }
    query[i] = '\0';

    // Restore cx and cy
    e->cx = cx;
    e->cy = cy;
    return query;
}
/* Search through a file with the editor.
 */
void editor_find(editor_t *e, const char *query)
{
    char *p;

    // Search through the file.
    if(e->find >= e->size - 1) {
        e->find = 0;
    }

    p = strstr(&e->data[e->find], query);

    if(p == NULL) {
        e->find = 0;
        p = strstr(&e->data[e->find], query);
    }

    if(p != NULL) {
        long unsigned offset = p - e->data;
        long lines = editor_getline(e, offset);
        long unsigned offset2;

        // Calculate cursor position in buffer.
        if(lines >= (e->rows - 2)) {
            e->skiprows = lines - (e->rows - 2);
        }
        else {
            e->skiprows = 0;
        }
        e->cy = lines - e->skiprows;
        offset2 = editor_getoffset(e, e->cy + e->skiprows);
        e->skipcols = (long)(offset - offset2) >= e->cols ? ((offset - offset2) - (e->cols - 1)) + strlen(query) : 0;
        e->cx = (long)(offset - offset2) >= e->cols ? ((offset - offset2) - e->skipcols) % e->cols : (offset - offset2);
        e->find = offset + strlen(query);
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
    if(at >= e->size) return;
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
    long unsigned startx = editor_getoffset(e, e->cy + e->skiprows);
    long unsigned endx = editor_getoffset(e, (e->cy + e->skiprows) + 1);

    if(e->linecount == 0 || (endx - startx) == 0) {
        _editor_inschr(e, at, '\n');
    }
    _editor_inschr(e, at, ch);
    editor_getlinecount(e);
}
/* Delete a line of text from the buffer.
 */
void editor_deleteline(editor_t *e, long line)
{
    long unsigned startx = editor_getoffset(e, line);
    long unsigned endx = editor_getoffset(e, line + 1);
    unsigned i;

    for(i = 0; i < (endx - startx); i++) {
        editor_delchr(e, startx);
    }
}
/* Clear a line on the screen.
 */
void editor_clearline(editor_t *e, long line, long col)
{
    register long i;

    if(line < 0 || line > e->rows - 1) return;
    if(col < 0 || col > e->cols - 1) return;

    for(i = col; i < e->cols; i++) {
        mvaddch(line, i, ' ');
    }
}
/* Render a line of text on the screen.
 */
void editor_renderline(editor_t *e, long line)
{
    long unsigned startx = editor_getoffset(e, line + e->skiprows);
    long unsigned endx = editor_getoffset(e, (line + e->skiprows) + 1);
    long size = (endx - startx) > 0 ? (endx - startx) - 1 : 0;
    register long i;

    if(line < 0 || line > e->rows - 1) return;

    if(has_colors())
        attron(COLOR_PAIR(EDITOR_PAIR));

    editor_clearline(e, line, 0);
    for(i = 0; i < size; i++) {
        if((!isprint(e->data[startx + i]) && !iscntrl(e->data[startx + i]))
                || (e->data[startx + i] == '\t'))
            mvaddch(line, i - e->skipcols, ' ');
        else
            mvaddch(line, i - e->skipcols, e->data[startx + i]);
    }
    if(i < e->cols)
        editor_clearline(e, line, i);

    if(has_colors())
        attroff(COLOR_PAIR(EDITOR_PAIR));
}
/* Render text buffer from editor.
 */
void editor_render(editor_t *e)
{
    int y;

    // Display the text on the screen from the editor buffer.
    for(y = 0; y < e->rows - 1; y++) {
        // Render line of text to screen.
        editor_renderline(e, y);
    }
}
/* Set status message for editor.
 */
void editor_setstatus(editor_t *e, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->status, sizeof(e->status), fmt, ap);
    va_end(ap);
}
/* Render status message on screen.
 */
void editor_renderstatus(editor_t *e)
{
    if(has_colors())
        attron(COLOR_PAIR(STATUS_PAIR));
    editor_clearline(e, e->rows - 1, 0);
    mvprintw(e->rows - 1, 0, e->status);
    if(has_colors())
        attroff(COLOR_PAIR(STATUS_PAIR));
}

/* ---------------------------- Main Functions ------------------------- */

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
    bool istab;
    editor_t e;
    int c;

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
    else {
        editor_convnewline(&e);
        editor_convtab(&e, false);
    }
    istab = false;
    editor_getlinecount(&e);
    ncurses_init();
    getmaxyx(stdscr, e.rows, e.cols);
    clear();
    editor_render(&e);
    editor_setstatus(&e, "Ctrl-Q: Exit | Ctrl-S: Save | Ctrl-F: Find "
        "| F3: Find Next | F5: Convert Tabs");
    editor_renderstatus(&e);
    move(e.cy, e.cx);

    while((c = getch()) != CTRL_KEY('q')) {
        long startx = 0;
        long endx = 0;

        // Resizing terminal screen.
        getmaxyx(stdscr, e.rows, e.cols);

        // Handle keyboard input.
        switch(c) {
            case CTRL_KEY('s'): {
                char status[80];
                int len;

                if(editor_save(&e, argv[1]) != 0) {
                    len = snprintf(status, sizeof(status),
                        "Error: Saving file %s.", argv[1]);
                }
                else {
                    len = snprintf(status, sizeof(status),
                        "Saving file %s totaling %ld bytes.",
                        argv[1], e.size);
                }

                // Draw message to status bar.
                editor_setstatus(&e, "%s", status);
                editor_renderstatus(&e);

                // Fill rest of buffer with STATUS_PAIR color.
                if(has_colors())
                    attron(COLOR_PAIR(STATUS_PAIR));
                editor_clearline(&e, e.rows - 1, len);
                if(has_colors())
                    attroff(COLOR_PAIR(STATUS_PAIR));
                refresh();
                e.status_on = true;
            } break;
            case CTRL_KEY('f'):
                // Find in file.
                e.find = 0;
                e.findstr = editor_findprompt(&e, "Find: ");
                if(e.findstr != NULL) {
                    editor_find(&e, e.findstr);
                }
                e.dirty = true;
            break;
            case CTRL_KEY('k'):
                // Delete current line.
                if(e.linecount > 0) {
                    editor_deleteline(&e, e.cy + e.skiprows);
                    e.skipcols = 0;
                    e.cx = 0;
                    e.dirty = true;
                }
            break;
            case KEY_F(3):
                // Find next in file.
                if(e.findstr != NULL) {
                    editor_find(&e, e.findstr);
                }
                e.dirty = true;
            break;
            case KEY_F(5):
                // Convert tabs to spaces and back again.
                istab = !istab;
                editor_convtab(&e, istab);
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
                if(e.cx <= (e.cols - 1) &&
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
                    (e.cx + e.skipcols) < (endx - startx) &&
                    (e.cy + e.skiprows) < e.linecount) {
                    editor_delchr(&e, startx + (e.cx + e.skipcols));
                }
                e.dirty = true;
            break;
            case KEY_BACKSPACE:
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
                    if(e.cy > 0)
                        e.cy--;
                    else
                        e.skiprows--;
                    editor_delchr(&e, startx + (e.cx + e.skipcols));
                }
                else if((e.cx + e.skipcols) == 0 && (e.cy + e.skiprows) == 0 &&
                    (e.cy + e.skiprows) < e.linecount) {
                    if(e.skiprows > 0) {
                        int len;

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
            editor_render(&e);
            refresh();
            e.dirty = false;
        }

        // Render status message.
        if(!e.status_on) {
            editor_setstatus(&e, "[%s] - Lines: %ld/%ld",
                argv[1], e.linecount != 0 ? (e.cy + e.skiprows) + 1 : 0,
                e.linecount);
            editor_renderstatus(&e);
        }

        // Reset status message.
        if(e.status_on)
            e.status_on = false;

        // Move cursor.
        move(e.cy, e.cx);
    }

    editor_free(&e);
    return 0;
}
