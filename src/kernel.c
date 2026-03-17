
#include "vga.h"
#include "gdt/gdt.h"
#include "idt/idt.h"
#include "keyboard.h"
#include "strings.h"
#include "shell.h"

void kmain() {
    
    gdt_init();
    idt_init();


    asm volatile("sti");   // enabling cpu interrupts
    keyboard_init();


    init_shell();

    
    for (;;) asm volatile("hlt"); // halt CPU and wait for interrupts
    
}
