/*
 * main.c - Source for a simple text editor.
 *
 * Author: Philip R. Simonson
 * Date  : 04/15/2021
 *
 *******************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

/* ------------------------ Raw Mode Code ------------------------- */

/* Show error and exit with failure.
 */
void die(const char *msg)
{
	write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
	perror(msg);
	exit(EXIT_FAILURE);
}
/* Disable raw mode.
 */
void disable_raw_mode(void)
{
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) < 0)
		die("tcsetattr");
}
/* Switch to raw input mode.
 */
void enable_raw_mode(void)
{
	struct termios raw;

	if(tcgetattr(STDIN_FILENO, &orig_termios) < 0) die("tcgetattr");
	atexit(disable_raw_mode);

	raw = orig_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) die("tcsetattr");
}
/* Get cursor position on screen.
 */
int get_cursor_position(int *y, int *x)
{
	char buf[32];
	unsigned i = 0;

	if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

	while(i < sizeof(buf) - 1) {
		if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if(buf[i] == 'R') break;
		i++;
	}
	buf[i] = 0;

	if(buf[0] != '\x1b' || buf[1] != '[') return -1;
	if(sscanf(&buf[2], "%d;%d", y, x) != 2) return -1;
	return 0;
}

/* ------------------------ Editor Code ------------------------- */

#define TO_STR(X) #X
#define STR(A) TO_STR(A)

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define PSEDIT_VERSION STR(VERSION_MAJOR) "." STR(VERSION_MINOR)

#define BUFFER_WIDTH 80
#define BUFFER_HEIGHT 24
#define FILENAME_LEN 1024

#define CTRL_KEY(X) ((X) & 0x1f)

enum editor_keys {
	ARROW_LEFT = 0,
	ARROW_RIGHT, 
	ARROW_UP,
	ARROW_DOWN
};

typedef struct editor {
	char filename[FILENAME_LEN]; // File name of current file
	unsigned char data[BUFFER_HEIGHT][BUFFER_WIDTH]; // Fixed screen buffer
	unsigned curx, cury; // Cursor X/Y
	unsigned scrx, scry; // Scroll X/Y
	unsigned long size;  // Size of data buffer
	bool dirty;          // Dirty flag (modified ??)
} editor_t;

struct abuf {
	char *b;
	unsigned long len;
};

#define ABUF_INIT { NULL, 0 } // Initialize append buffer

/* Append buffer.
 */
void ab_append(struct abuf *ab, const char *s, unsigned long len)
{
	char *new = realloc(ab->b, ab->len+len);

	if(new != NULL) {
		memcpy(&new[ab->len], s, len);
		ab->b = new;
		ab->len += len;
	}
}
/* Free append buffer.
 */
void ab_free(struct abuf *ab)
{
	if(ab == NULL) return;

	free(ab->b);
	ab->b = NULL;
	ab->len = 0;
}
/* Editor reset.
 */
void editor_reset(editor_t *ed)
{
	memset(ed->filename, 0, FILENAME_LEN);
	memset(ed->data, 0, BUFFER_WIDTH*BUFFER_HEIGHT);
	ed->curx = ed->cury = 0;
	ed->scrx = ed->scry = 0;
	ed->dirty = 0;
}
/* Editor draw rows.
 */
void editor_draw_rows(editor_t *ed, struct abuf *ab)
{
	int y;

	for(y = 0; y < BUFFER_HEIGHT; y++) {
		if(y == BUFFER_HEIGHT / 3) {
			char w[80];
			int wlen, padding;
			wlen = snprintf(w, sizeof(w), "PS Edit %s",
				PSEDIT_VERSION);
			if(wlen > BUFFER_WIDTH) wlen = BUFFER_WIDTH;
			padding = (BUFFER_WIDTH - wlen) / 2;
			if(padding) {
				ab_append(ab, "~", 1);
				padding--;
			}
			while(padding--) ab_append(ab, " ", 1);
			ab_append(ab, w, wlen);
		} else {
			ab_append(ab, "~", 1);
		}

		ab_append(ab, "\x1b[K", 3);
		if(y < BUFFER_HEIGHT - 1) {
			ab_append(ab, "\r\n", 2);
		}
	}
}
/* Editor redraw screen.
 */
void editor_redraw(editor_t *ed)
{
	struct abuf ab = ABUF_INIT;

	ab_append(&ab, "\x1b[?25l", 6);
	ab_append(&ab, "\x1b[H", 3); // reset cursor

	editor_draw_rows(ed, &ab);

	ab_append(&ab, "\x1b[H", 3);
	ab_append(&ab, "\x1b[?25h", 6);
	write(STDOUT_FILENO, ab.b, ab.len);
	ab_free(&ab);
}
/* Read key from keyboard.
 */
char editor_read_key(void)
{
	int nread;
	char c;

	while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if(nread < 0 && errno != EAGAIN) die("read");
	}

	if(c == '\x1b') {
		char seq[3];

		if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

		if(seq[0] == '[') {
			switch(seq[1]) {
			case 'A': return ARROW_UP;
			case 'B': return ARROW_DOWN;
			case 'C': return ARROW_RIGHT;
			case 'D': return ARROW_LEFT;
			}
		}

		return '\x1b';
	} else {
		return c;
	}
	return -1; // Never gets here
}
/* Move cursor position with arrow keys.
 */
void editor_move_cursor(editor_t *ed, char key)
{
	switch(key) {
	case ARROW_LEFT:
		if(ed->curx != 0)
			ed->curx--;
	break;
	case ARROW_RIGHT:
		if(ed->curx != BUFFER_WIDTH-1)
			ed->curx++;
	break;
	case ARROW_UP:
		if(ed->cury != 0)
			ed->cury--;
	break;
	case ARROW_DOWN:
		if(ed->cury != BUFFER_HEIGHT-1)
			ed->cury++;
	break;
	default:
	break;
	}
}
/* Get user input from keyboard.
 */
void editor_process_input(editor_t *ed)
{
	char c = editor_read_key();

	switch(c) {
	case CTRL_KEY('q'):
		write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
		exit(0);
	break; // Never gets here

	// Arrow keys
	case ARROW_UP:
	case ARROW_DOWN:
	case ARROW_LEFT:
	case ARROW_RIGHT:
		editor_move_cursor(ed, c);
	break;
	}
}
/* Load file into buffer.
 */
int editor_load_file(editor_t *ed, const char *name)
{
	return EXIT_SUCCESS;
}

/* ------------------------ Main Program ------------------------ */

/* Entry point for text editor.
 */
int main(void)
{
	editor_t ed;
	enable_raw_mode();
	editor_reset(&ed);

	for(;;) {
		editor_redraw(&ed);
		editor_process_input(&ed);
	}
	return EXIT_SUCCESS;
}
