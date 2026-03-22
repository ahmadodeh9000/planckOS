#include "syscall.h"
#include "common.h"


/* handling system calls*/
void syscall_handler(registers_t* regs) {
    switch (regs->eax)
    {
    case SYS_PRINT:
        print((char*)regs->ebx);
        break;
    
    default:
        break;
    }

}
