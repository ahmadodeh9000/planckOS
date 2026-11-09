/*
 * io.h - x86 port I/O.
 *
 * x86 talks to most legacy devices (PIC, PIT, PS/2, ATA, UART, VGA registers)
 * through a separate 64 KB I/O address space using the in/out instructions.
 * These wrappers are static inline so each call compiles to one instruction.
 */
#ifndef io_h__
#define io_h__

#include "lib/common.h"

static inline void outportb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" :: "a"(value), "Nd"(port));
}

static inline uint8_t inportb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outportw(uint16_t port, uint16_t value) {
    asm volatile("outw %0, %1" :: "a"(value), "Nd"(port));
}

static inline uint16_t inportw(uint16_t port) {
    uint16_t value;
    asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

#endif
