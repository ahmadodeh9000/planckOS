#include "idt.h"
#include "../util.h"
#include "../syscall.h"

#define IDT_ENTRIES 256


extern void idt_load(idt_pointer*);

static idt_entry idt_entries[IDT_ENTRIES];
static idt_pointer idtp;

void* irq_routines[16] = {
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
};

void irq_install_handler(int irq, void(*handler)(registers_t *r)) {
    irq_routines[irq] = handler;
    
}

void irq_uninstall_handler(int irq) {
    irq_routines[irq] = 0;
}



void irq_handler(registers_t *r) {
    void(*handler)(registers_t *r);

    handler = irq_routines[r->int_no - 32];
   
    if (handler) {
        handler(r);
    }
       // print("this shit fucked");
    if (r->int_no >= 40) {
        outportb(0xA0,0x20);
    }

    outportb(0x20,0x20);
}

const char* exception_messages[32] = {
    "Division By Zero",                // 0
    "Debug",                           // 1
    "Non Maskable Interrupt",          // 2
    "Breakpoint",                      // 3
    "Into Detected Overflow",          // 4
    "Out of Bounds",                   // 5
    "Invalid Opcode",                  // 6
    "No Coprocessor",                  // 7
    "Double Fault",                    // 8
    "Coprocessor Segment Overrun",     // 9
    "Bad TSS",                         // 10
    "Segment Not Present",             // 11
    "Stack Fault",                     // 12
    "General Protection Fault",        // 13
    "Page Fault",                      // 14
    "Unknown Interrupt",               // 15
    "Coprocessor Fault",               // 16
    "Alignment Check",                 // 17
    "Machine Check",                   // 18
    "SIMD Floating-Point Exception",   // 19
    "Reserved 20",                     // 20
    "Reserved 21",                     // 21
    "Reserved 22",                     // 22
    "Reserved 23",                     // 23
    "Reserved 24",                     // 24
    "Reserved 25",                     // 25
    "Reserved 26",                     // 26
    "Reserved 27",                     // 27
    "Reserved 28",                     // 28
    "Reserved 29",                     // 29
    "Security Exception",              // 30
    "Reserved 31"                      // 31
};


void isr_handler(registers_t* regs) {
    if (regs->int_no < 32) {
        print(exception_messages[regs->int_no]);
        print("\n");
        print("EXCEPTION !!\nplanckOS has been halted !");
        for (;;) asm("hlt");
    }

    if (regs->int_no == 128) {
        syscall_handler(regs);
    }
}

void set_idt_gate(int32_t n ,uint32_t handler) {
    idt_entries[n].low_offset           = handler & 0xFFFF;
    idt_entries[n].selector             = 0x08;                     /* kernel code segment */
    idt_entries[n].zero                 = 0x00;
    idt_entries[n].type_attributees     = 0x8E;                     /* Interrupt gate */
    idt_entries[n].high_offset          = (handler >> 16) & 0xFFFF;
}

void set_idt_gate_user(int32_t n ,uint32_t handler) {
    idt_entries[n].low_offset           = handler & 0xFFFF;
    idt_entries[n].selector             = 0x08;                     /* kernel code segment */
    idt_entries[n].zero                 = 0x00;
    idt_entries[n].type_attributees     = 0xEE;                     /* Interrupt gate */
    idt_entries[n].high_offset          = (handler >> 16) & 0xFFFF;
}

void idt_init() {
    idtp.limit = (sizeof(idt_entry) * IDT_ENTRIES) - 1;
    idtp.base  = (uint32_t) &idt_entries;

    kmemset(idt_entries,0, sizeof(idt_entries));

    outportb(0x20,0x11);
    outportb(0xA0,0x11);

    outportb(0x21,0x20);
    outportb(0xA1,0x28);

    outportb(0x21,0x04);
    outportb(0xA1,0x02);

    outportb(0x21,0x01);
    outportb(0xA1,0x01);

    outportb(0x21,0x0);
    outportb(0xA1,0x0);

    set_idt_gate(0,  (uint32_t)isr0);
    set_idt_gate(1,  (uint32_t)isr1);
    set_idt_gate(2,  (uint32_t)isr2);
    set_idt_gate(3,  (uint32_t)isr3);
    set_idt_gate(4,  (uint32_t)isr4);   
    set_idt_gate(5,  (uint32_t)isr5);
    set_idt_gate(6,  (uint32_t)isr6);
    set_idt_gate(7,  (uint32_t)isr7);

    set_idt_gate(8,  (uint32_t)isr8);
    set_idt_gate(9,  (uint32_t)isr9);
    set_idt_gate(10, (uint32_t)isr10);
    set_idt_gate(11, (uint32_t)isr11);
    set_idt_gate(12, (uint32_t)isr12);
    set_idt_gate(13, (uint32_t)isr13);
    set_idt_gate(14, (uint32_t)isr14);
    set_idt_gate(15, (uint32_t)isr15);

    set_idt_gate(16, (uint32_t)isr16);
    set_idt_gate(17, (uint32_t)isr17);
    set_idt_gate(18, (uint32_t)isr18);
    set_idt_gate(19, (uint32_t)isr19);
    set_idt_gate(20, (uint32_t)isr20);
    set_idt_gate(21, (uint32_t)isr21);
    set_idt_gate(22, (uint32_t)isr22);
    set_idt_gate(23, (uint32_t)isr23);

    set_idt_gate(24, (uint32_t)isr24);
    set_idt_gate(25, (uint32_t)isr25);
    set_idt_gate(26, (uint32_t)isr26);
    set_idt_gate(27, (uint32_t)isr27);
    set_idt_gate(28, (uint32_t)isr28);
    set_idt_gate(29, (uint32_t)isr29);
    set_idt_gate(30, (uint32_t)isr30);
    set_idt_gate(31, (uint32_t)isr31);

    set_idt_gate_user(128,(uint32_t) isr128);

    // IRQ0–IRQ15 (timer, keyboard, etc.)
    set_idt_gate(32, (uint32_t)irq0);
    set_idt_gate(33, (uint32_t)irq1);
    set_idt_gate(34, (uint32_t)irq2);
    set_idt_gate(35, (uint32_t)irq3);
    set_idt_gate(36, (uint32_t)irq4);
    set_idt_gate(37, (uint32_t)irq5);
    set_idt_gate(38, (uint32_t)irq6);
    set_idt_gate(39, (uint32_t)irq7);
    set_idt_gate(40, (uint32_t)irq8);
    set_idt_gate(41, (uint32_t)irq9);
    set_idt_gate(42, (uint32_t)irq10);
    set_idt_gate(43, (uint32_t)irq11);
    set_idt_gate(44, (uint32_t)irq12);
    set_idt_gate(45, (uint32_t)irq13);
    set_idt_gate(46, (uint32_t)irq14);
    set_idt_gate(47, (uint32_t)irq15);

    idt_load(&idtp);
}