#include "vga.h"
#include "gdt.h"
#include "idt.h"
//#include "timer.h"
#include "keyboard.h"

void kmain() {
    print_rainbow("PlanckOS V 0.0.1\n\n");
    
    gdt_init();
    idt_init();



    //timer_install();
    init_keyboard();
    asm volatile("sti");
    for (;;) asm volatile("hlt"); // halt CPU and wait for interrupts
    
}
