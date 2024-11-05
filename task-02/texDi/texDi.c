/*** includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/
struct editorConfig {
	int cx, cy;
	int screenrows;
  	int screencols;
  	struct termios orig_termios;
};
struct editorConfig E;

/*** terminal ***/
void die(const char *s) {
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	perror(s);																// prints the descriptive error message along with context of which part caused the error
	exit(1);
}
void disableRawMode() {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)				// TCSAFLUSH is to determine when to apply the changes. Here, to wait for pending output to be on the terminal and discard all other inputs which was not read
		die("tcsetattr");
}
void enableRawMode() {			
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");	// canonical/cooked mode is where the input is fed only when the user hits enter
																			// get the terminal attributes and then save it to orig_termios struct
	atexit(disableRawMode);													// at-exit is to run the disabling function automatically
	struct termios raw = E.orig_termios;										// make a copy of the original struct to make changes to into raw mode
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);				// iflag - input flag:
																			// 1. IXON refers to XOFF and XON transmission produced while using CTRL-S and CTRL-Q to pause and resume sending outputs
																			// 2. ICRNL - inputflag carriage return to new line
																			// 3. BRKINT is a breaking condition causing a SIGINT signal to be sent to the program like CTRL+C
																			// 4. INPCK - enables parity checking (error check)
																			// 5. ISTRIP - causes the 8th bit of every byte to be stripped to 0
	raw.c_oflag &= ~(OPOST);												// oflag - output flag: negation of OPOST (output post-processing) is to not mix the ASCII of CTRL+M, \r and \n to be 10 
	raw.c_cflag |= (CS8);													// cflag - control flag: CS8 sets the character size to 8 bits per byte
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);						// lflag - local flag:
  																			// 1. negation of ECHO is to not print the keys as we type (bitwise AND operator is to forcefully make the forth bit 0)
																			// 2. negation of ICANON disables the canonical mode and finally lets us to read byte-by-byte instead of line-by-line
																			// 3. negation of ISIG stops sending the signal to terminal to stop and suspending the program while typing CTRL-C and CTRL-Z
																			// 4. negation of IEXTEN means it is not implementation-defined (implementation is free to do what it likes, but must document its choice and stick to it) anymore because of CTRL-V waiting for you to type another character sometimes
	raw.c_cc[VMIN] = 0;														// cc (control characters) which is an array of bytes that control various terminal settings
	raw.c_cc[VTIME] = 1;													// maximum amount of time before read() returns - 1/10th of seconds
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");	// set the new attributes according to the raw struct
}
char editorReadKey() {
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) die("read");
	}


  	if (c == '\x1b') {
    	char seq[3];
    	if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    	if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    	if (seq[0] == '[') {
    	  	switch (seq[1]) {
    	    	case 'A': return 'w';
    	    	case 'B': return 's';
    	    	case 'C': return 'd';
    	    	case 'D': return 'a';
    	  	}
    	}
    	return '\x1b';
  	} else {
		return c;
	}
}
int getCursorPosition(int *rows, int *cols) {
	char buf[32];
  	unsigned int i = 0;

  	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  	while (i < sizeof(buf) - 1) {
    	if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    	if (buf[i] == 'R') break;
    	i++;
  	}
  	buf[i] = '\0';

  	if (buf[0] != '\x1b' || buf[1] != '[') return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
	
	return 0;
}
int getWindowSize(int *rows, int *cols) {
  	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
	return getCursorPosition(rows, cols);
  	} else {
    	*cols = ws.ws_col;
    	*rows = ws.ws_row;
    	return 0;
  	}
}

/*** append buffer ***/
struct abuf {
	char *b;
	int len;
};
#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
	char *new = realloc(ab->b, ab->len + len);
	if (new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}
void abFree(struct abuf *ab) {
	free(ab->b);
}

/*** output ***/
void editorDrawRows(struct abuf *ab) {
	int y;
	for (y = 0; y < E.screenrows; y++) if (y == E.screenrows / 3) {
    	char welcome[80];
    	int welcomelen = snprintf(welcome, sizeof(welcome), "texDi, the text editor");
    	if (welcomelen > E.screencols) welcomelen = E.screencols;
		int padding = (E.screencols - welcomelen) / 2;
      	if (padding) {
        	abAppend(ab, "~", 1);
        	padding--;
      	}
      	while (padding--) abAppend(ab, " ", 1);
    		abAppend(ab, welcome, welcomelen);
    	} else {
      		abAppend(ab, "~", 1);
    	}

		abAppend(ab, "\x1b[K", 3);
    	if (y < E.screenrows - 1) {
    		abAppend(ab, "\r\n", 2);
    	}
}
void editorRefreshScreen() {
	struct abuf ab = ABUF_INIT;

	abAppend(&ab, "\x1b[?25l", 6);
	abAppend(&ab, "\x1b[H", 3);

	editorDrawRows(&ab);

	char buf[32];
  	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  	abAppend(&ab, buf, strlen(buf));

	abAppend(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
}

/*** input ***/
void editorMoveCursor(char key) {
  	switch (key) {
  	  	case 'a':
  	  	  	E.cx--;
  	  	  	break;
  	  	case 'd':
  	  	  	E.cx++;
  	  	  	break;
  	  	case 'w':
  	  	  	E.cy--;
  	  	  	break;
  	  	case 's':
  	  	  	E.cy++;
  	  	  	break;
  	}
}
void editorProcessKeypress() {
	char c = editorReadKey();
	switch (c) {
    	case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
      		write(STDOUT_FILENO, "\x1b[H", 3);
      		exit(0);
      		break;
		case 'w':
    	case 's':
    	case 'a':
    	case 'd':
      		editorMoveCursor(c);
      		break;
  	}
}

/*** init ***/
void initEditor() {
  	E.cx = 0;
  	E.cy = 0;

	if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}
int main() {
	enableRawMode();
	initEditor();

	while (1) {
		editorRefreshScreen();
		editorProcessKeypress();
	}
	return 0;
}