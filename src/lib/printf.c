/*
 * printf.c - minimal printf on top of the VGA text driver.
 *
 * Both functions share one formatter that is given a set of output
 * functions, so the plain and rainbow versions don't duplicate the
 * format-string parsing.
 */
#include "lib/printf.h"
#include "drivers/vga.h"
#include <stdarg.h>

typedef struct {
    void (*put_char)(char c);
    void (*put_string)(const char* str);
    void (*put_number)(int number);
    void (*put_hex)(uint32_t number);
} printf_sink;

static const printf_sink plain_sink = { putc, print, print_number, print_hex };
static const printf_sink rainbow_sink = {
    putc_rainbow, print_rainbow, print_number_rainbow, print_hex_rainbow
};

static void format(const printf_sink* out, const char* fmt, va_list args) {
    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            out->put_char(fmt[i]);
            continue;
        }
        i++;
        switch (fmt[i]) {
            case 's': out->put_string(va_arg(args, char*)); break;
            case 'c': out->put_char((char)va_arg(args, int)); break;
            case 'd': out->put_number(va_arg(args, int)); break;
            case 'x': out->put_hex(va_arg(args, unsigned int)); break;
            case '\0': return;     /* lone '%' at the end */
            default:
                out->put_char('%');
                out->put_char(fmt[i]);
        }
    }
}

void printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    format(&plain_sink, fmt, args);
    va_end(args);
}

void printf_rainbow(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    format(&rainbow_sink, fmt, args);
    va_end(args);
}
