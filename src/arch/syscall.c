/*
 * syscall.c - handles int 0x80 (called from isr_handler in idt.c).
 *
 * Writing to regs->eax sets the return value: the stub restores registers
 * from this struct before iretd.
 */
#include "arch/syscall.h"
#include "lib/console.h"
#include "mm/kmalloc.h"

void syscall_handler(registers_t* regs) {
    switch (regs->eax) {
        case SYS_PRINT:
            printf((char*)regs->ebx);
            break;
        case SYS_PRINT_HEX:
            printf("%x\n", regs->ebx);
            break;
        case SYS_MALLOC:
            regs->eax = (uint32_t)kmalloc(regs->ebx);
            break;
        case SYS_FREE:
            kfree((void*)regs->ebx);
            break;
        case SYS_EXIT:
            printf("Process ended\n");
            for (;;) asm volatile("hlt");
        default:
            break;
    }
}
