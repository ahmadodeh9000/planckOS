/*
 * syscall.h - system call numbers for int 0x80.
 *
 * Put the number in eax and the argument in ebx; results come back in eax.
 * programs/planck.h has the same numbers for programs; keep them in sync.
 */
#ifndef syscall_h__
#define syscall_h__

#include "arch/idt.h"

#define SYS_PRINT       0
#define SYS_MALLOC      1
#define SYS_FREE        2
#define SYS_EXIT        3       /* ebx = exit code */
#define SYS_PRINT_HEX   4

void syscall_handler(registers_t* regs);

#endif
