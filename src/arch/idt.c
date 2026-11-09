/*
 * idt.c - builds the IDT, remaps the 8259 PICs and dispatches interrupts.
 *
 * The assembly stubs in idt.s save the CPU state into a registers_t and
 * call isr_handler() (exceptions and syscalls) or irq_handler() (hardware
 * IRQs). Drivers hook IRQs with irq_install_handler().
 */
#include "arch/idt.h"
#include "arch/syscall.h"
#include "lib/console.h"
#include "drivers/serial.h"
#include "lib/string.h"

#define IDT_ENTRIES         256
#define EXCEPTION_COUNT     32
#define IRQ_COUNT           16
#define IRQ_BASE_VECTOR     32      /* IRQ 0 -> vector 32 after remapping */
#define SYSCALL_VECTOR      0x80

#define KERNEL_CODE_SELECTOR    0x08
#define GATE_KERNEL_INTERRUPT   0x8E    /* present, ring 0, 32-bit interrupt gate */
#define GATE_USER_INTERRUPT     0xEE    /* same, but callable from ring 3         */

/* 8259 PIC ports and commands */
#define PIC_MASTER_CMD      0x20
#define PIC_MASTER_DATA     0x21
#define PIC_SLAVE_CMD       0xA0
#define PIC_SLAVE_DATA      0xA1
#define PIC_EOI             0x20
#define PIC_ICW1_INIT       0x11    /* start init, expect ICW4             */
#define PIC_ICW3_SLAVE_AT_2 0x04    /* master: a slave sits on IRQ 2       */
#define PIC_ICW3_CASCADE_ID 0x02    /* slave: your cascade identity is 2   */
#define PIC_ICW4_8086       0x01
#define PIC_SLAVE_FIRST_IRQ 8

typedef struct {
    uint16_t low_offset;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attributes;       /* gate type, DPL and present bit */
    uint16_t high_offset;
} __attribute__((packed)) idt_entry;

typedef struct {
    uint16_t limit;                 /* size of the IDT in bytes, minus one */
    uint32_t base;
} __attribute__((packed)) idt_pointer;

/* Assembly entry points from idt.s */
extern void idt_load(idt_pointer* ptr);
extern void isr0(void),  isr1(void),  isr2(void),  isr3(void),  isr4(void),  isr5(void);
extern void isr6(void),  isr7(void),  isr8(void),  isr9(void),  isr10(void), isr11(void);
extern void isr12(void), isr13(void), isr14(void), isr15(void), isr16(void), isr17(void);
extern void isr18(void), isr19(void), isr20(void), isr21(void), isr22(void), isr23(void);
extern void isr24(void), isr25(void), isr26(void), isr27(void), isr28(void), isr29(void);
extern void isr30(void), isr31(void), isr128(void);
extern void irq0(void),  irq1(void),  irq2(void),  irq3(void),  irq4(void),  irq5(void);
extern void irq6(void),  irq7(void),  irq8(void),  irq9(void),  irq10(void), irq11(void);
extern void irq12(void), irq13(void), irq14(void), irq15(void);

static void (*const exception_stubs[EXCEPTION_COUNT])(void) = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
};

static void (*const irq_stubs[IRQ_COUNT])(void) = {
    irq0, irq1, irq2,  irq3,  irq4,  irq5,  irq6,  irq7,
    irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15,
};

static const char* const exception_messages[EXCEPTION_COUNT] = {
    "Division By Zero",             "Debug",
    "Non Maskable Interrupt",       "Breakpoint",
    "Into Detected Overflow",       "Out of Bounds",
    "Invalid Opcode",               "No Coprocessor",
    "Double Fault",                 "Coprocessor Segment Overrun",
    "Bad TSS",                      "Segment Not Present",
    "Stack Fault",                  "General Protection Fault",
    "Page Fault",                   "Unknown Interrupt",
    "Coprocessor Fault",            "Alignment Check",
    "Machine Check",                "SIMD Floating-Point Exception",
    "Reserved 20",                  "Reserved 21",
    "Reserved 22",                  "Reserved 23",
    "Reserved 24",                  "Reserved 25",
    "Reserved 26",                  "Reserved 27",
    "Reserved 28",                  "Reserved 29",
    "Security Exception",           "Reserved 31",
};

static idt_entry idt_entries[IDT_ENTRIES];
static idt_pointer idtp;
static irq_handler_t irq_handlers[IRQ_COUNT];

void irq_install_handler(int irq, irq_handler_t handler) {
    irq_handlers[irq] = handler;
}

/* Called from irq_common_stub in idt.s. */
void irq_handler(registers_t* regs) {
    irq_handler_t handler = irq_handlers[regs->int_no - IRQ_BASE_VECTOR];

    if (handler) {
        handler(regs);
    }

    /* Tell the PIC(s) we are done, or that IRQ line stays blocked. IRQs 8-15
       come through the slave, which needs its own end-of-interrupt too. */
    if (regs->int_no >= IRQ_BASE_VECTOR + PIC_SLAVE_FIRST_IRQ) {
        outportb(PIC_SLAVE_CMD, PIC_EOI);
    }
    outportb(PIC_MASTER_CMD, PIC_EOI);
}

/* Called from isr_common_stub in idt.s. */
void isr_handler(registers_t* regs) {
    if (regs->int_no < EXCEPTION_COUNT) {
        serial_printf("EXCEPTION %d (%s) at eip %x, error %x\n", regs->int_no,
                      exception_messages[regs->int_no], regs->eip, regs->err_code);
        printf("%s\nEXCEPTION !!\nplanckOS has been halted !", exception_messages[regs->int_no]);
        for (;;) asm volatile("hlt");
    }

    if (regs->int_no == SYSCALL_VECTOR) {
        syscall_handler(regs);
    }
}

static void idt_set_gate(int n, void (*handler)(void), uint8_t type_attributes) {
    uint32_t address = (uint32_t)handler;

    idt_entries[n].low_offset      = address & 0xFFFF;
    idt_entries[n].selector        = KERNEL_CODE_SELECTOR;
    idt_entries[n].zero            = 0;
    idt_entries[n].type_attributes = type_attributes;
    idt_entries[n].high_offset     = (address >> 16) & 0xFFFF;
}

/*
 * By default the PICs deliver IRQs 0-7 on vectors 8-15, which collide with
 * CPU exceptions (8 = double fault). Re-initialize both PICs so IRQs 0-15
 * land on vectors 32-47, then unmask every line.
 */
static void pic_remap(void) {
    outportb(PIC_MASTER_CMD, PIC_ICW1_INIT);
    outportb(PIC_SLAVE_CMD, PIC_ICW1_INIT);

    outportb(PIC_MASTER_DATA, IRQ_BASE_VECTOR);
    outportb(PIC_SLAVE_DATA, IRQ_BASE_VECTOR + PIC_SLAVE_FIRST_IRQ);

    outportb(PIC_MASTER_DATA, PIC_ICW3_SLAVE_AT_2);
    outportb(PIC_SLAVE_DATA, PIC_ICW3_CASCADE_ID);

    outportb(PIC_MASTER_DATA, PIC_ICW4_8086);
    outportb(PIC_SLAVE_DATA, PIC_ICW4_8086);

    outportb(PIC_MASTER_DATA, 0x00);    /* interrupt masks: all enabled */
    outportb(PIC_SLAVE_DATA, 0x00);
}

void idt_init(void) {
    idtp.limit = sizeof(idt_entries) - 1;
    idtp.base  = (uint32_t)&idt_entries;
    memset(idt_entries, 0, sizeof(idt_entries));

    pic_remap();

    for (int i = 0; i < EXCEPTION_COUNT; ++i) {
        idt_set_gate(i, exception_stubs[i], GATE_KERNEL_INTERRUPT);
    }
    idt_set_gate(SYSCALL_VECTOR, isr128, GATE_USER_INTERRUPT);
    for (int i = 0; i < IRQ_COUNT; ++i) {
        idt_set_gate(IRQ_BASE_VECTOR + i, irq_stubs[i], GATE_KERNEL_INTERRUPT);
    }

    idt_load(&idtp);
}
