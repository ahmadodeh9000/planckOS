/*
 * planck.h - everything a planckOS program needs to talk to the kernel.
 *
 * Programs are linked to run in the program area (see program.ld) and are
 * called directly by the kernel's 'exec' command, in ring 0. They ask the
 * kernel for services with int 0x80: the call number goes in eax and the
 * argument in ebx. The numbers must match src/arch/syscall.h.
 */
#ifndef planck_h__
#define planck_h__

#include <stdint.h>

#define SYS_PRINT       0
#define SYS_EXIT        3
#define SYS_PRINT_HEX   4
#define SYS_PRINT_NUMBER 5

/* Print 'text' in the Terminal that ran the program. */
static inline void sys_print(const char* text) {
    asm volatile("int $0x80" :: "a"(SYS_PRINT), "b"(text) : "memory");
}

/* Print 'value' as hex, followed by a newline. */
static inline void sys_print_hex(uint32_t value) {
    asm volatile("int $0x80" :: "a"(SYS_PRINT_HEX), "b"(value) : "memory");
}

/* Print 'value' in decimal (no newline). */
static inline void sys_print_number(int32_t value) {
    asm volatile("int $0x80" :: "a"(SYS_PRINT_NUMBER), "b"(value) : "memory");
}

/* End the program right away with 'code'; the shell reports non-zero codes.
   Returning from _start does the same with the return value. */
static inline void sys_exit(uint32_t code) {
    asm volatile("int $0x80" :: "a"(SYS_EXIT), "b"(code) : "memory");
    __builtin_unreachable();
}

#endif
