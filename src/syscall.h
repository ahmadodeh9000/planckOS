#ifndef syscall_h__
#define syscall_h__

#include "util.h"

/*          SYSCALLS            */
#define SYS_PRINT       0
#define SYS_MALLOC      1
#define SYS_FREE        2
#define SYS_EXIT        3
#define SYS_PRINT_HEX   4

void syscall_handler(registers_t* regs);




#endif