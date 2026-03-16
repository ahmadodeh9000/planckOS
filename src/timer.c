#include "idt.h"
#include "vga.h"

volatile uint32_t ticks = 0;

void timer_handler(registers_t *r) {
    ticks++;
    if (ticks % 100 == 0) {  // print every 100 ticks
        print("Ticks: ");
        print_number(ticks);
        print("\n");
    }
}

void timer_install() {
    uint32_t divisor = 1193180 / 100;
    outportb(0x43, 0x36);
    outportb(0x40, divisor & 0xFF);
    outportb(0x40, (divisor >> 8) & 0xFF);
    

    irq_install_handler(0, timer_handler);
    
}