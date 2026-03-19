#include "util.h"




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
