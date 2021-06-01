/*
 * main.c - Source file for my simple text editor.
 *
 * Author: Philip R. Simonson
 * Date  : 05/25/2021
 *
 ****************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ncurses.h>
#include <unistd.h>
#include <ctype.h>

#define MAXHEIGHT 25
#define MAXWIDTH 80

#define KEY_CONTROL(x) ((x) & 0x1F)
#define KEY_RETURN 0x0A

/* Cleanup and report error.
 */
void die(const char *message)
{
    endwin();
    fprintf(stderr, "%s", message);
    exit(1);
}
/* Get offset from skiplines.
 */
long getoffset(const char *buffer, int skiplines)
{
    long i = 0;
    int lines = 0;
    while(lines < skiplines && buffer[i] != 0) {
        if(buffer[i] == '\n')
            lines++;
        i++;
    }
    return i;
}
/* Get the total amount of lines in the current file.
 */
int getlinecount(const char *buffer)
{
    int i = 0, lines = 0;
    while(buffer[i] != 0) {
        if(buffer[i] == '\n')
            lines++;
        i++;
    }
    return lines;
}
/* Delete a character from the buffer.
 */
void delchar(char *buffer, long *size, int at, long offset)
{
    if(at < 0 || at > *size) return;
    memcpy(&buffer[at], &buffer[at + 1], offset-at);
    *size -= 1;
}
/* Insert a character into the buffer.
 */
void inschar(char **buffer, long *size, int at, long offset, int ch)
{
    char *tmp;

    if(at < 0 || at >= *size) return;

    tmp = realloc(*buffer, sizeof(char) * (*size + 2));
    if(tmp != NULL) {
        memcpy(&tmp[at + 1], &tmp[at], offset-at+1);
        tmp[at] = ch;
        *buffer = tmp;
        *size += 1;
    }
}
/* Render a line of text to the screen.
 */
void renderlines(const char *buffer, int skiplines)
{
    int x, y;

    for(y = 0; y < MAXHEIGHT; y++) {
        long startx = getoffset(buffer, y + skiplines);
        long endx = getoffset(buffer, (y + skiplines) + 1);
        long len = ((endx-startx) < MAXWIDTH ? (endx-startx) : MAXWIDTH);
        for(x = 0; x < len; x++) {
            mvaddch(y, x, buffer[startx+x]);
        }
    }
}
/* Main function for text editor.
 */
int main(int argc, char **argv)
{
    char *buffer, config[128];
    long total, size;
    FILE *fp;
    int skiplines;
    int linecount;
    int term_backup;
    long offset;
    bool dirty;
    int x, y;
    int c;

    // Check for arguments
    if(argc != 2) {
	    printf("Usage: %s <filename>\n", argv[0]);
	    return 1;
    }

    // Open config file for reading, if config don't
    // exist than create it.
    strncpy(config, getenv("HOME"), sizeof(config)-1);
    strncat(config, "/.config/psedit/config", sizeof(config)-1);
    fp = fopen(config, "r");
    if(fp == NULL) {
        char cmd[256], buf[256];

        fprintf(stderr, "Warning: Could not open config file %s\nCreating config file...\n", config);
        snprintf(cmd, sizeof(cmd), "mkdir -p %s/.config/psedit", getenv("HOME"));

        // Make directory for config file
        if(system(cmd)) {
            fprintf(stderr, "Error: Could not create directory!\n");
            return 1;
        }

        // Open config file for writing
        fp = fopen(config, "w");
        if(fp == NULL) {
            fprintf(stderr, "Error: Cannot create config file %s!\n", config);
            return 1;
        }
        fprintf(fp, "BACKUP_FILE=0\n");

        // Reopen config file for reading
        fp = freopen(config, "r", fp);
        if(fp == NULL) {
            fprintf(stderr, "Error: Cannot open config file for reading!\n");
            return 1;
        }

        while(fgets(buf, sizeof(buf), fp)) {
            if(sscanf(buf, "BACKUP_FILE=%d", &term_backup) != 1) {
                fprintf(stderr, "Warning cannot get color scheme setting defaults.\n");
                term_backup = 1; // Set default term backup
            }
        }
        fclose(fp);

        if(term_backup < 0 || term_backup > 1)
            term_backup = 1; // Set default term backup
    }
    else { // Parse config file
        char buf[256];

        while(fgets(buf, sizeof(buf), fp)) {
            if(sscanf(buf, "BACKUP_FILE=%d", &term_backup) != 1) {
                fprintf(stderr, "Warning: cannot get backup file setting.\n");
                term_backup = 1; // Set default term backup
            }
        }
        fclose(fp);

        if(term_backup < 0 || term_backup > 1)
            term_backup = 1; // Set default term backup
    }

    // Open file for reading and get it's total size
    fp = fopen(argv[1], "r");
    if(fp == NULL) {
        fprintf(stderr, "Error: Could not open file %s!\n", argv[1]);
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    // Allocate memory for buffer and fill the buffer
    buffer = malloc(sizeof(char) * (size + 1));
    if(buffer == NULL) {
        fprintf(stderr, "Error: Could not allocate memory for buffer.\n");
        return 1;
    }
    total = fread(buffer, sizeof(char), size, fp);
    fclose(fp);
    if(total != size) {
        fprintf(stderr, "Error: Could not load the entire file.\n"
                "Only %ld/%ld bytes read.\n", total, size);
        return 1;
    }

    // Initialise ncurses library
    initscr();
    noecho();
    cbreak();
    raw();
    keypad(stdscr, TRUE);
    clear();

    // Initialise skiplines to zero and main loop for editor
    offset = 0;
    dirty = false;
    skiplines = 0;
    getyx(stdscr, y, x);
    linecount = getlinecount(buffer);
    renderlines(buffer, skiplines);
    move(0, 0);
    refresh();

    while((c = getch()) != KEY_CONTROL('q')) {
        long startx, endx;

        startx = endx = 0;

        // Handle key input
        switch(c) {
            case KEY_CONTROL('s'): // Save buffer to file and make backup
                if(term_backup) {
                    char path[256];
                    char *backbuf;
                    long size2;

                    // Open original file for reading
                    fp = fopen(argv[1], "r");
                    if(fp == NULL) {
                        fprintf(stderr, "Warning: Could not open original file.\n");
                        continue;
                    }
                    fseek(fp, 0, SEEK_END);
                    size2 = ftell(fp);
                    rewind(fp);
                    backbuf = malloc(sizeof(char) * size2);
                    if(backbuf == NULL) {
                        fclose(fp);
                    }
                    total = fread(backbuf, 1, size2, fp);
                    fclose(fp);
                    if(total != size2) {
                        fprintf(stderr, "Error: Could not read in original file.\n");
                        free(backbuf);
                        continue;
                    }
                    snprintf(path, sizeof(path), "%s.bak", argv[1]);
                    fp = fopen(path, "w");
                    if(fp == NULL) {
                        fprintf(stderr, "Warning: could not save file!\n");
                        free(backbuf);
                        continue;
                    }
                    total = fwrite(backbuf, 1, size2, fp);
                    free(backbuf);
                    fclose(fp);
                    if(total != size2) {
                        fprintf(stderr, "Error: Could not write backup file.\n");
                        continue;
                    }
                }

                // Backup created successfully continue with saving.
                fp = fopen(argv[1], "w");
                if(fp == NULL) {
                    fprintf(stderr, "Error: Could not open original file for writing.\n");
                    continue;
                }
                total = fwrite(buffer, 1, size, fp);
                fclose(fp);
                if(total != size) {
                    fprintf(stderr, "Error: Could not save file.");
                }
            break;
            case KEY_UP: // Up Arrow - Move up document and scroll
                if(y > 0) {
                    y--;
                }
                if(y <= 0) {
                    dirty = true;
                    if(skiplines > 0) {
                        skiplines--;
                    }
                }
            break;
            case KEY_DOWN: // Down Arrow - Move down document and scroll
                // Please replace if needed...
                if(y < MAXHEIGHT-1 && y < (linecount - 1)) {
                    y++;
                }
                if(y >= MAXHEIGHT-1) {
                    dirty = true;
                    if(skiplines < linecount-1) {
                        skiplines++;
                    }
                }
            break;
            case KEY_LEFT: // Left Arrow
                if(x != 0) {
                    x--;
                }
            break;
            case KEY_RIGHT: // Right Arrow
                startx = getoffset(buffer, y + skiplines);
                endx = getoffset(buffer, (y + skiplines) + 1);
                if(x < (endx-startx)-1) {
                    x++;
                }
            break;
            case KEY_HOME: // Home
                offset = getoffset(buffer, y + skiplines);
                if(x > 0) {
                    x = 0;
                }
            break;
            case KEY_END: // End
                startx = getoffset(buffer, y + skiplines);
                endx = getoffset(buffer, (y + skiplines) + 1);
                if(x < (endx-startx) - 1) {
                    x = (endx - startx) - 1;
                }
            break;
            case KEY_PPAGE: // Page up
                if(y >= 0 && skiplines > 0) {
                    if(skiplines > 10)
                        skiplines -= 10;
                    else
                        skiplines--;
                    dirty = true;
                }
            break;
            case KEY_NPAGE: // Page down
                if(y <= MAXHEIGHT-1 && skiplines < (linecount - 1)) {
                    if(skiplines < (linecount - 1) - 10)
                        skiplines += 10;
                    else
                        skiplines++;
                    dirty = true;
                }
            break;
            case KEY_DC: // Delete
                startx = getoffset(buffer, y + skiplines);
                endx = getoffset(buffer, (y + skiplines) + 1);
                if(x >= 0 && x < (endx - startx)) {
                    int len = getoffset(buffer, linecount - 1);
                    delchar(buffer, &size, startx + x, len);
                    linecount = getlinecount(buffer);
                    dirty = true;
                }
            break;
            case KEY_BACKSPACE: // Backspace
                startx = getoffset(buffer, y + skiplines);
                endx = getoffset(buffer, (y + skiplines) + 1);
                if(x > 0 && x < (endx - startx)) {
                    int len = getoffset(buffer, linecount - 1);
                    delchar(buffer, &size, startx + (x - 1), len);
                    linecount = getlinecount(buffer);
                    dirty = true;
                    x--;
                } else {
                    int len = getoffset(buffer, linecount - 1);
                    delchar(buffer, &size, startx + x, len);
                    if(y > 0) {
                        y--;
                        x = (endx-startx);
                    }
                }
            break;
            case KEY_RETURN: // Enter key
            case KEY_ENTER:
                if(y >= 0 && y < linecount - 1) {
                    startx = getoffset(buffer, y + skiplines);
                    endx = getoffset(buffer, (y + skiplines) + 1);
                    if(x >= 0 && x < (endx - startx)) {
                        int len = getoffset(buffer, linecount - 1);
                        inschar(&buffer, &size, startx + x, len, '\n');
                        linecount = getlinecount(buffer);
                        dirty = true;
                        y++;
                        x = 0;
                    }
                }
            break;
            default:
                // Process only the printable characters
                if(isprint(c)) {
                    startx = getoffset(buffer, y + skiplines);
                    endx = getoffset(buffer, (y + skiplines) + 1);
                    if(x >= 0 && x < (endx - startx)) {
                        int len = getoffset(buffer, linecount - 1);
                        inschar(&buffer, &size, startx + x, len, c);
                        linecount = getlinecount(buffer);
                        dirty = true;
                        x++;
                    }
                }
            break;
        }

        // Repaint if dirty (modified or scrolled)
        if(dirty) {
            clear();
            renderlines(buffer, skiplines);
            move(y, x);
            refresh();
            dirty = false;
        }
        else {
            // Update cursor
            move(y, x);
        }
    }

    // Cleanup resources and ncurses
    clear();
    endwin();
    free(buffer);
    return 0;
}

