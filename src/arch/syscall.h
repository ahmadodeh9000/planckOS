/*
 * syscall.h - system call numbers for int 0x80.
 *
 * Calling convention:
 *   eax           = call number; the result (if any) comes back in eax
 *   ebx ecx edx   = arguments 1, 2, 3
 *   esi edi       = arguments 4, 5
 * Every other register is preserved.
 *
 * programs/planck.h has the same numbers for programs; keep them in sync.
 */
#ifndef syscall_h__
#define syscall_h__

#include "arch/idt.h"

#define SYS_PRINT           0
#define SYS_MALLOC          1
#define SYS_FREE            2
#define SYS_EXIT            3       /* ebx = exit code */
#define SYS_PRINT_HEX       4
#define SYS_PRINT_NUMBER    5       /* ebx as a signed decimal, no newline */
#define SYS_WIN_CREATE      6       /* ebx = title, ecx = width, edx = height -> id / 0 */
#define SYS_WIN_ON          7       /* ebx = id, ecx = event, edx = function -> 1 ok / 0 */
#define SYS_WIN_RECT        8       /* ebx = id, ecx = x|y, edx = w|h, esi = color */
#define SYS_WIN_TEXT        9       /* ebx = id, ecx = x|y, edx = text, esi = color */
#define SYS_WIN_INVALIDATE  10      /* ebx = id: repaint the window soon */
#define SYS_WIN_CLOSE       11      /* ebx = id: close the window at the next frame */

/* Events for SYS_WIN_ON. "x|y" means (x << 16) | y in one register. */
#define WIN_EVENT_PAINT     0       /* paint(id) */
#define WIN_EVENT_KEY       1       /* key(id, key) */
#define WIN_EVENT_MOUSE     2       /* mouse(id, x, y): left button pressed */
#define WIN_EVENT_TIMER     3       /* timer(id): once a second */

void syscall_handler(registers_t* regs);

#endif
