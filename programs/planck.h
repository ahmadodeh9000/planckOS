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

#define SYS_PRINT           0
#define SYS_MALLOC          1
#define SYS_FREE            2
#define SYS_EXIT            3
#define SYS_PRINT_HEX       4
#define SYS_PRINT_NUMBER    5

#define SYS_WIN_CREATE      6
#define SYS_WIN_ON          7
#define SYS_WIN_RECT        8
#define SYS_WIN_TEXT        9
#define SYS_WIN_INVALIDATE  10

/* Events for SYS_WIN_ON. */
#define WIN_EVENT_PAINT     0       /* paint(id) */
#define WIN_EVENT_KEY       1       /* key(id, key) */
#define WIN_EVENT_MOUSE     2       /* mouse(id, x, y): left button pressed */
#define WIN_EVENT_TIMER     3       /* timer(id): once a second */

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

/* Open a window. Returns its id, or 0 if it couldn't be opened. */
static inline uint32_t sys_win_create(const char* title, int width, int height) {
    uint32_t id;
    asm volatile("int $0x80"
                 : "=a"(id)
                 : "a"(SYS_WIN_CREATE), "b"(title), "c"(width), "d"(height)
                 : "memory");
    return id;
}

/* Pack two 16-bit values into one register argument. */
#define PACK16(high, low)   (((uint32_t)(high) << 16) | ((uint32_t)(low) & 0xFFFF))

static inline uint32_t sys_win_on(uint32_t id, uint32_t event, void (*fn)()) {
    uint32_t ok;
    asm volatile("int $0x80" : "=a"(ok)
                 : "a"(SYS_WIN_ON), "b"(id), "c"(event), "d"(fn) : "memory");
    return ok;
}

static inline void sys_win_rect(uint32_t id, int x, int y, int w, int h, uint32_t color) {
    asm volatile("int $0x80" :: "a"(SYS_WIN_RECT), "b"(id),
                 "c"(PACK16(x, y)), "d"(PACK16(w, h)), "S"(color) : "memory");
}

static inline void sys_win_text(uint32_t id, int x, int y, const char* text, uint32_t color) {
    asm volatile("int $0x80" :: "a"(SYS_WIN_TEXT), "b"(id),
                 "c"(PACK16(x, y)), "d"(text), "S"(color) : "memory");
}

#endif
