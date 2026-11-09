/*
 * console.h - text output that doesn't care where the text ends up.
 *
 * A console_t is a "sink" for characters: anything that can show a
 * character and clear itself. The VGA text screen is one sink
 * (vga_console()), and each Terminal window is another. Code that prints,
 * like the shell, only talks to a console_t, so it runs unchanged in
 * text mode and in a window.
 */
#ifndef console_h__
#define console_h__

#include "lib/common.h"

/* Colors are indexes into the standard 16-color VGA palette. */
enum console_color {
    CONSOLE_BLACK, CONSOLE_BLUE, CONSOLE_GREEN, CONSOLE_CYAN,
    CONSOLE_RED, CONSOLE_MAGENTA, CONSOLE_BROWN, CONSOLE_LIGHT_GREY,
    CONSOLE_DARK_GREY, CONSOLE_LIGHT_BLUE, CONSOLE_LIGHT_GREEN, CONSOLE_LIGHT_CYAN,
    CONSOLE_LIGHT_RED, CONSOLE_LIGHT_MAGENTA, CONSOLE_YELLOW, CONSOLE_WHITE,
};
#define CONSOLE_COLOR_COUNT     16
#define CONSOLE_COLOR_DEFAULT   CONSOLE_WHITE

typedef struct console console_t;

struct console {
    /* Show one character. '\n' starts a new line, '\b' erases the
       character before the cursor. */
    void (*put_char)(console_t* con, char c, uint8_t color);
    void (*clear)(console_t* con);
    void* data;                         /* the sink's own state */
};

void console_putc(console_t* con, char c);
void console_print(console_t* con, const char* text);
void console_clear(console_t* con);

/* Print 'text' in one of the console_color colors. */
void console_print_color(console_t* con, const char* text, uint8_t color);

/* Each character gets the next color of the rainbow. */
void console_print_rainbow(console_t* con, const char* text);

/* Supports %s %c %d %u %x and %%. */
void console_printf(console_t* con, const char* fmt, ...);

/* The console that plain printf() writes to (kernel messages, syscalls). */
void console_set_default(console_t* con);
void printf(const char* fmt, ...);

#endif
