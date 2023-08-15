/* Includes */

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

/* Defines */
#define EDITOR_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

/* Data */
struct editorConfig
{
    struct termios original_attributes;
    int screenrows, screencols;
    int cx, cy;
};

struct editorConfig e;

/* Terminal */

void killProgram(const char* s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disableRawmode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &e.original_attributes) == -1)
        killProgram("tcsetattr");
}

void enableRawmode() {
    if(tcgetattr(STDIN_FILENO, &e.original_attributes) == -1) 
        killProgram("tcgetattr");
    
    atexit(disableRawmode);

    struct termios raw_attributes = e.original_attributes;

    raw_attributes.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw_attributes.c_oflag &= ~(OPOST);
    raw_attributes.c_cflag |= (CS8);
    raw_attributes.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw_attributes.c_cc[VMIN] = 0;
    raw_attributes.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_attributes) == -1) 
        killProgram("tcsetattr");
}

char editorReadKey() {
    int nread;
    char c;

    while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if(nread == -1 && errno != EAGAIN) killProgram("read");
    }

    return c;
}

int getCursorPosition(int *rows, int *cols) {
    char buffer[32];
    unsigned int i = 0;
    
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
    
    while(i < sizeof(buffer) - 1) {
        if(read(STDIN_FILENO, &buffer[i], 1) != 1) break;
        if(buffer[i] == 'R') break;

        i++;
    }

    buffer[i] = '\0';

    printf("\r\n&buffer[1]: '%s'\r\n", &buffer[1]);
    
    editorReadKey();
    
    if(buffer[0] != '\x1b' || buffer[1] != '[') return -1;
    if(sscanf(&buffer[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int * rows, int * cols) {
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/* Append Buffer */

struct abuf {
    char *buffer;
    int length;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf * ab, const char * s, int length) {
    char * new = realloc(ab->buffer, ab->length + length);

    if(new == NULL) return;
    memcpy(&new[ab->length], s, length);
    ab->buffer = new;
    ab->length += length;
}

void abFree(struct abuf *ab) {
    free(ab->buffer);
}

/* Input */

void editorMoveCursor(char key) {
    switch (key)
    {
    case 'w':
        e.cy--;
        break;
    case 'a':
        e.cx--;
        break;
    case 's':
        e.cy++;
        break;
    case 'd':
        e.cx++;
        break;
    }
}

void editorProcessKeypress() {
    char c = editorReadKey();

    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);

        exit(0);
        break;

        case 'w':
        case 'a':
        case 's':
        case 'd':
            editorMoveCursor(c);
            break;
    }
}

/* Output */

void editorDrawRows(struct abuf *ab) {
    int y;
    
    for (y = 0; y < e.screenrows; y++) {
        if(y == e.screenrows / 3) {
            char welcome_msg[80];
            int welcome_length = snprintf(welcome_msg, sizeof(welcome_msg), "Text Editor -- version %s", EDITOR_VERSION);
            if(welcome_length > e.screencols) welcome_length = e.screencols;
            int padding = (e.screencols - welcome_length) / 2;
            if(padding) {
                abAppend(ab, "~", 1);
                padding--;
            }
            while(padding--) abAppend(ab, " ", 1);
            abAppend(ab, welcome_msg, welcome_length);
        } else {
            abAppend(ab, "~", 1);
        }

        abAppend(ab, "\x1b[K", 3);
        if(y < e.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
    
}

void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;
    
    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", e.cy + 1, e.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.buffer, ab.length);
    abFree(&ab);
}

/* Init */

void initEditor() {
    e.cx = 0;
    e.cy = 0;

    if (getWindowSize(&e.screenrows, &e.screencols) == -1) killProgram("getWindowSize");
}

int main() {
    enableRawmode();
    initEditor();
    
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}