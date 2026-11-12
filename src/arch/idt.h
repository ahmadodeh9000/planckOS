/*
 * idt.h - interrupt descriptor table, PIC remapping and IRQ dispatch.
 *
 * Exceptions (vectors 0-31) halt the kernel with a message, vector 0x80 is
 * the system call gate, and hardware IRQs 0-15 are remapped to vectors
 * 32-47 and forwarded to handlers registered with irq_install_handler().
 */
#ifndef idt_h__
#define idt_h__

#include "lib/common.h"
#include "arch/io.h"

/* Register snapshot built by the assembly stubs in idt.s, in stack order. */
typedef struct {
    uint32_t gs, fs, es, ds;            /* pushed last by the stub          */
    uint32_t edi, esi, ebp, esp,
             ebx, edx, ecx, eax;        /* pusha                            */
    uint32_t int_no, err_code;          /* pushed by the ISR/IRQ macro      */
    uint32_t eip, cs, eflags,
             useresp, ss;               /* pushed by the CPU                */
} __attribute__((packed)) registers_t;

typedef void (*irq_handler_t)(registers_t* regs);

void idt_init(void);
void irq_install_handler(int irq, irq_handler_t handler);

/* Human-readable name of CPU exception 'vector' (0-31). */
const char* idt_exception_name(uint32_t vector);

#endif
