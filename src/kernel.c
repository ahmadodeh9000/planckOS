
#include "vga.h"
#include "gdt/gdt.h"
#include "idt/idt.h"
#include "keyboard.h"
#include "strings.h"
#include "shell.h"
#include "pmm.h"
#include "paging.h"
#include "kmalloc.h"
#include "syscall.h"

extern void user_app();



void kmain(uint32_t magic, uint32_t mbi_addr) {
    
    gdt_init();
    idt_init();


    asm volatile("sti");   // enabling cpu interrupts
    pmm_init((multiboot_info_t *)mbi_addr);  // <-- add this
    paging_init();
    heap_init();
   
    user_app();


    for (;;) asm volatile("hlt"); // halt CPU and wait for interrupts
    
}
