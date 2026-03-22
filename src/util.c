#include "util.h"
#include <stdarg.h>

#include <stdarg.h>

void printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%') {
            i++;

            switch (fmt[i]) {
                case 's': {
                    char* str = va_arg(args, char*);
                    print(str);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    putc(c);
                    break;
                }
                case 'd': {
                    int num = va_arg(args, int);
                    print_number(num);
                    break;
                }
                case 'x': {
                    unsigned int num = va_arg(args, unsigned int);
                    print_hex(num);
                    break;
                }
                default:
                    putc('%');
                    putc(fmt[i]);
            }
        } else {
            putc(fmt[i]);
        }
    }

    va_end(args);
}

void printf_rainbow(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%') {
            i++;

            switch (fmt[i]) {
                case 's': {
                    char* str = va_arg(args, char*);
                    print_rainbow(str);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    putc_rainbow(c);
                    break;
                }
                case 'd': {
                    int num = va_arg(args, int);
                    print_number_rainbow(num);
                    break;
                }
                case 'x': {
                    unsigned int num = va_arg(args, unsigned int);
                    print_hex_rainbow(num);
                    break;
                }
                default:
                    putc_rainbow('%');
                    putc_rainbow(fmt[i]);
            }
        } else {
            putc_rainbow(fmt[i]);
        }
    }

    va_end(args);
}


void* kmemset(void* ptr, int32_t value, uint32_t count) {
    uint32_t* p = ptr;
    
    while (--count) {
        *p = (uint32_t) value;
    }

    return p;
}

void outportb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

char inportb(uint16_t port) {
    char rv;
    __asm__ volatile ("inb %1, %0" : "=a"(rv) : "Nd"(port));
    return rv;

}

uint16_t inportw(uint16_t port) {
    uint16_t result;
    asm volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void outportw(uint16_t port, uint16_t value) {
    asm volatile("outw %0, %1" :: "a"(value), "Nd"(port));
}
