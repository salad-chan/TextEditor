/* Includes */

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

/* Data */

struct termios original_attributes;

/* Terminal */

void killProgram(const char* s) {
    perror(s);
    exit(1);
}

void disableRawmode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_attributes) == -1)
        killProgram("tcsetattr");
}

void enableRawmode() {
    if(tcgetattr(STDIN_FILENO, &original_attributes) == -1) 
        killProgram("tcgetattr");
    
    atexit(disableRawmode);

    struct termios raw_attributes = original_attributes;

    raw_attributes.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw_attributes.c_oflag &= ~(OPOST);
    raw_attributes.c_cflag |= (CS8);
    raw_attributes.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw_attributes.c_cc[VMIN] = 0;
    raw_attributes.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_attributes) == -1) 
        killProgram("tcsetattr");
}

/* Init */

int main() {
    enableRawmode();
    
    while (1) {
        char c = '\0';

        if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
            killProgram("read");

        if(iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }

        if(c == 'q') break;
    }
    
    return 0;
}