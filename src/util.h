#ifndef util_h__
#define util_h__

#include "common.h"
#include "vga.h"

void* kmemset(void* ptr, int32_t value, uint32_t count);
void outportb(uint16_t port, uint8_t value);
char inportb(uint16_t port);
void printf(const char* fmt, ...);
void printf_rainbow(const char* fmt, ...);

#define __debug__

typedef struct {
    uint32_t gs, fs, es, ds;          // pushed last (popped first)
    uint32_t edi, esi, ebp, esp,
             ebx, edx, ecx, eax;      // pusha (reversed)
    uint32_t int_no, err_code;        // our manual pushes
    uint32_t eip, cs, eflags,
             useresp, ss;             // pushed by CPU on interrupt
} __attribute__((packed)) registers_t;





#endif