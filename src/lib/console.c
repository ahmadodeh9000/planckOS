/*
 * console.c - printing helpers on top of a console_t sink.
 */
#include "lib/console.h"
#include "lib/string.h"
#include <stdarg.h>

#define RAINBOW_COLORS  15      /* palette colors 1-15; 0 is black */
#define NUMBER_MAX      12

static console_t* default_console;

void console_putc(console_t* con, char c) {
    con->put_char(con, c, CONSOLE_COLOR_DEFAULT);
}

void console_print(console_t* con, const char* text) {
    while (*text) {
        console_putc(con, *text++);
    }
}

void console_clear(console_t* con) {
    con->clear(con);
}

void console_print_color(console_t* con, const char* text, uint8_t color) {
    while (*text) {
        con->put_char(con, *text++, color);
    }
}

void console_print_rainbow(console_t* con, const char* text) {
    for (int i = 0; text[i]; i++) {
        con->put_char(con, text[i], i % RAINBOW_COLORS + 1);
    }
}

static void buffer_put_char(console_t* con, char c, uint8_t color) {
    (void)color;
    console_buffer_t* b = con->data;
    if (c == '\b') {
        if (b->length > 0) {
            b->length--;
        }
    } else if (b->length + 1 < b->capacity) {
        b->text[b->length++] = c;
    }
    b->text[b->length] = '\0';
}

static void buffer_clear(console_t* con) {
    console_buffer_t* b = con->data;
    b->length = 0;
    b->text[0] = '\0';
}

void console_buffer_init(console_buffer_t* b, char* text, uint32_t capacity) {
    b->console.put_char = buffer_put_char;
    b->console.clear = buffer_clear;
    b->console.data = b;
    b->text = text;
    b->capacity = capacity;
    buffer_clear(&b->console);
}

static void console_print_signed(console_t* con, int32_t value) {
    char digits[NUMBER_MAX];

    if (value < 0) {
        console_putc(con, '-');
        console_print(con, utoa(-(uint32_t)value, digits, 10));
    } else {
        console_print(con, utoa(value, digits, 10));
    }
}

static void console_format(console_t* con, const char* fmt, va_list args) {
    char digits[NUMBER_MAX];

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            console_putc(con, *fmt);
            continue;
        }
        switch (*++fmt) {
            case 's': console_print(con, va_arg(args, const char*)); break;
            case 'c': console_putc(con, (char)va_arg(args, int)); break;
            case 'd': console_print_signed(con, va_arg(args, int32_t)); break;
            case 'u': console_print(con, utoa(va_arg(args, uint32_t), digits, 10)); break;
            case 'x':
                console_print(con, "0x");
                console_print(con, utoa(va_arg(args, uint32_t), digits, 16));
                break;
            case '%': console_putc(con, '%'); break;
            case '\0': return;      /* lone '%' at the end */
            default:
                console_putc(con, '%');
                console_putc(con, *fmt);
        }
    }
}

void console_printf(console_t* con, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    console_format(con, fmt, args);
    va_end(args);
}

void console_set_default(console_t* con) {
    default_console = con;
}

void printf(const char* fmt, ...) {
    if (!default_console) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    console_format(default_console, fmt, args);
    va_end(args);
}
