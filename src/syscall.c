#include "syscall.h"
#include "common.h"
#include "kmalloc.h"

/* handling system calls*/
void syscall_handler(registers_t* regs) {
    switch (regs->eax)
    {
        case SYS_PRINT:
            printf((char*)regs->ebx);
            break;
        
        case SYS_PRINT_HEX:
            printf("%x\n",(char*)regs->ebx);
            break;

        case SYS_MALLOC:
            regs->eax = (uint32_t ) kmalloc(regs->ebx);
            break;
        case SYS_FREE:
            kfree((void*) regs->ebx);
            break;
        
        case SYS_EXIT:
            printf("Process ended\n");
            while(1) asm volatile("hlt");
            break;
    
        default:
            break;
    }

}
