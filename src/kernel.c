#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "timer.h"


void kmain() {
    gdt_init();
    idt_init();
    timer_install();

    asm volatile("sti");
    for (;;) asm volatile("hlt"); // halt CPU and wait for interrupts
    
}
