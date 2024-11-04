/*** includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** data ***/

struct termios orig_termios;

/*** terminal ***/

void die(const char *s) {
	perror(s);																// prints the descriptive error message along with context of which part caused the error
	exit(1);
}

void disableRawMode() {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == 1)				// TCSAFLUSH is to determine when to apply the changes. Here, to wait for pending output to be on the terminal and discard all other inputs which was not read
		die("tcsetattr");
}	void enableRawMode() {			
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");		// canonical/cooked mode is where the input is fed only when the user hits enter
	tcgetattr(STDIN_FILENO, &orig_termios);									// get the terminal attributes and then save it to orig_termios struct
	atexit(disableRawMode);													// at-exit is to run the disabling function automatically
	struct termios raw = orig_termios;										// make a copy of the original struct to make changes to into raw mode
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

/*** init ***/

int main() {
	enableRawMode();
	
	while (1) {
		char c = '\0';
		if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
		if (iscntrl(c)) {									// to check whether 'c' is a control character (cannot be printed) [ACSII 0 to 31]
			printf("%d\r\n", c);									
		} else {
			printf("%d ('%c')\r\n", c, c);					// print with ASCII value
		} if (c == 'q') break;
	}
	return 0;
}