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
#include "exec/progwin.h"
#include "drivers/rtc.h"
#include "mm/kmalloc.h"

/* Two signed 16-bit values packed into one register: (high << 16) | low.
   Going through int16_t keeps the sign, so x = -5 arrives as -5, not 65531,
   and a program can draw shapes that start left of or above the window. */
#define HIGH16(v)   ((int)(int16_t)((v) >> 16))
#define LOW16(v)    ((int)(int16_t)((v) & 0xFFFF))

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

/* The time of day from the RTC, packed as (hour << 16) | (minute << 8) | second. */
static uint32_t syscall_time(void) {
    rtc_time_t now;
    rtc_read(&now);
    return ((uint32_t)now.hour << 16) | ((uint32_t)now.minute << 8) | now.second;
}

/* SYS_WIN_* (6-12): windows for programs, see exec/progwin.c. */
static void syscall_window(registers_t* regs) {
    switch (regs->eax) {
        case SYS_WIN_CREATE:
            regs->eax = progwin_create((const char*)regs->ebx,
                                       (int)regs->ecx, (int)regs->edx);
            break;
        case SYS_WIN_ON:
            regs->eax = progwin_set_callback(regs->ebx, regs->ecx, regs->edx);
            break;
        case SYS_WIN_RECT:
            progwin_fill_rect(regs->ebx, HIGH16(regs->ecx), LOW16(regs->ecx),
                              HIGH16(regs->edx), LOW16(regs->edx), regs->esi);
            break;
        case SYS_WIN_TEXT:
            progwin_draw_text(regs->ebx, HIGH16(regs->ecx), LOW16(regs->ecx),
                              (const char*)regs->edx, regs->esi);
            break;
        case SYS_WIN_INVALIDATE:
            progwin_invalidate(regs->ebx);
            break;
        case SYS_WIN_CLOSE:
            progwin_close(regs->ebx);
            break;
        case SYS_WIN_LINE:
            progwin_draw_line(regs->ebx, HIGH16(regs->ecx), LOW16(regs->ecx),
                              HIGH16(regs->edx), LOW16(regs->edx), regs->esi);
            break;
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
        case SYS_TIME:
            regs->eax = syscall_time();
            break;
        case SYS_EXIT:
            /* Ends the program and returns to whoever started it; only
               comes back here if no program is running. */
            program_exit(regs->ebx);
            serial_printf("syscall: SYS_EXIT with no program running\n");
            break;
        default:
            syscall_window(regs);
            break;
    }
}
