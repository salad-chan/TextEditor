#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

struct termios original_attributes;

void disableRawmode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_attributes);
}

void enableRawmode() {
    tcgetattr(STDIN_FILENO, &original_attributes);
    atexit(disableRawmode);

    struct termios raw_attributes = original_attributes;

    raw_attributes.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_attributes);
}

int main() {
    enableRawmode();

    char c;
    
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
    
    return 0;
}