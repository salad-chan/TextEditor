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

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  PAGE_UP,
  PAGE_DOWN,
  HOME_KEY,
  END_KEY,
  DELETE_KEY
};

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

int editorReadKey() {
    int nread;
    char c;

    while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if(nread == -1 && errno != EAGAIN) killProgram("read");
    }

    if(c == '\x1b') {
        char sequence[3];

        if(read(STDIN_FILENO, &sequence[0], 1) != 1) return '\x1b';
        if(read(STDIN_FILENO, &sequence[1], 1) != 1) return '\x1b';

        if(sequence[0] == '[') {
            if(sequence[1] >= '0' && sequence[1] <= '9') {
                if(read(STDIN_FILENO, &sequence[2], 1) != 1) return '\x1b';
                if(sequence[2] == '~') {
                    switch (sequence[1])
                    {
                    case '1':
                        return HOME_KEY;
                        break;
                    case '3':
                        return DELETE_KEY;
                        break;
                    case '4':
                        return END_KEY;
                        break;
                    case '5':
                        return PAGE_UP;
                        break;
                    case '6':
                        return PAGE_DOWN;
                        break;
                    case '7':
                        return HOME_KEY;
                        break;
                    case '8':
                        return END_KEY;
                        break;
                    }
                }
            } else {
                switch (sequence[1]) {
                case 'A': 
                    return ARROW_UP; 
                    break;
                case 'B': 
                    return ARROW_DOWN; 
                    break;
                case 'C': 
                    return ARROW_RIGHT; 
                    break;
                case 'D': 
                    return ARROW_LEFT; 
                    break;
                case 'H':
                    return HOME_KEY;
                    break;
                case 'F':
                    return END_KEY;
                    break;
                }
            }
        } else if (sequence[0] == 'O') {
            switch (sequence[1]) {
            case 'H':
                return HOME_KEY;
                break;
            case 'F':
                return END_KEY;
                break;
            }
        }
        

        return '\x1b';

    } else {
        return c;
    }
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

void editorMoveCursor(int key) {
    switch (key)
    {
    case ARROW_UP:
        if(e.cy != 0) {
            e.cy--;
        }
        break;
    case ARROW_LEFT:
        if(e.cx != 0) {
            e.cx--;
        }
        break;
    case ARROW_DOWN:
        if(e.cy != e.screenrows - 1) {
            e.cy++;
        }
        break;
    case ARROW_RIGHT:
        if(e.cx != e.screencols - 1) {
            e.cx++;
        }
        break;
    }
}

void editorProcessKeypress() {
    int c = editorReadKey();

    switch (c)
    {
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);

        exit(0);
        break;
    case ARROW_UP:
    case ARROW_LEFT:
    case ARROW_DOWN:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
    case PAGE_UP:
    case PAGE_DOWN:
        {
            int times = e.screenrows;
            while(times--)
                editorMoveCursor( ( c == PAGE_UP ) ? ARROW_UP : ARROW_DOWN);
        }
        break;
    case HOME_KEY:
        e.cx = 0;
        break;
    case END_KEY:
        e.cx = e.screencols - 1;
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