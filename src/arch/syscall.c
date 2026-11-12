/*
 * syscall.c - handles int 0x80 (called from isr_handler in idt.c).
 *
 * Writing to regs->eax sets the return value: the stub restores registers
 * from this struct before iretd.
 *
 * Output from a running program goes to the console of the Terminal (or
 * text screen) that started it; outside a program it goes to the default
 * console.
 */
#include "arch/syscall.h"
#include "exec/program.h"
#include "drivers/serial.h"
#include "lib/console.h"
#include "mm/kmalloc.h"

static void syscall_print(const char* text) {
    console_t* con = program_console();
    if (con) {
        console_print(con, text);
    } else {
        printf("%s", text);
    }
}

static void syscall_print_hex(uint32_t value) {
    console_t* con = program_console();
    if (con) {
        console_printf(con, "%x\n", value);
    } else {
        printf("%x\n", value);
    }
}

static void syscall_print_number(int32_t value) {
    console_t* con = program_console();
    if (con) {
        console_printf(con, "%d", value);
    } else {
        printf("%d", value);
    }
}

void syscall_handler(registers_t* regs) {
    switch (regs->eax) {
        case SYS_PRINT:
            syscall_print((const char*)regs->ebx);
            break;
        case SYS_PRINT_HEX:
            syscall_print_hex(regs->ebx);
            break;
        case SYS_PRINT_NUMBER:
            syscall_print_number((int32_t)regs->ebx);
            break;
        case SYS_MALLOC:
            regs->eax = (uint32_t)kmalloc(regs->ebx);
            break;
        case SYS_FREE:
            kfree((void*)regs->ebx);
            break;
        case SYS_EXIT:
            /* Ends the program and returns to whoever started it; only
               comes back here if no program is running. */
            program_exit(regs->ebx);
            serial_printf("syscall: SYS_EXIT with no program running\n");
            break;
        default:
            break;
    }
}
