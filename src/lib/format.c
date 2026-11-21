/*
 * format.c - snprintf for the kernel (see format.h).
 *
 * Every character goes through put(), which counts it but stores it only
 * while there's room. That one rule gives snprintf's return value (the
 * full length) and its truncation for free.
 */
#include "lib/format.h"
#include "lib/string.h"

#define DIGITS_MAX  12      /* "-2147483648" and its '\0' */

typedef struct {
    char* out;
    uint32_t capacity;
    uint32_t length;        /* characters produced so far, stored or not */
} format_t;

static void put(format_t* f, char c) {
    if (f->length + 1 < f->capacity) {
        f->out[f->length] = c;
    }
    f->length++;
}

static void put_text(format_t* f, const char* text) {
    while (*text) {
        put(f, *text++);
    }
}

static void put_signed(format_t* f, int32_t value) {
    char digits[DIGITS_MAX];

    if (value < 0) {
        put(f, '-');
        /* -(uint32_t) also works for -2147483648, which has no positive int */
        put_text(f, utoa(-(uint32_t)value, digits, 10));
    } else {
        put_text(f, utoa(value, digits, 10));
    }
}

/* One conversion after a '%'. Returns where the format continues. */
static const char* put_conversion(format_t* f, const char* fmt, va_list* args) {
    char digits[DIGITS_MAX];

    if (*fmt == 'l' && fmt[1] == 'd') {
        put_signed(f, (int32_t)va_arg(*args, long));
        return fmt + 2;
    }
    switch (*fmt) {
        case 's': put_text(f, va_arg(*args, const char*)); break;
        case 'c': put(f, (char)va_arg(*args, int)); break;
        case 'd': put_signed(f, va_arg(*args, int32_t)); break;
        case 'u': put_text(f, utoa(va_arg(*args, uint32_t), digits, 10)); break;
        case '%': put(f, '%'); break;
        case '\0': return fmt;      /* a lone '%' at the end */
        default:                    /* unknown: show it as it is */
            put(f, '%');
            put(f, *fmt);
    }
    return fmt + 1;
}

int kvsnprintf(char* out, uint32_t capacity, const char* fmt, va_list args) {
    format_t f = { out, capacity, 0 };
    va_list copy;

    va_copy(copy, args);        /* passed by pointer, so a va_list we own */
    while (*fmt) {
        if (*fmt == '%') {
            fmt = put_conversion(&f, fmt + 1, &copy);
        } else {
            put(&f, *fmt++);
        }
    }
    va_end(copy);
    if (capacity > 0) {
        out[f.length < capacity ? f.length : capacity - 1] = '\0';
    }
    return (int)f.length;
}

int ksnprintf(char* out, uint32_t capacity, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int length = kvsnprintf(out, capacity, fmt, args);
    va_end(args);
    return length;
}
