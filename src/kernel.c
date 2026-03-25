
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
#include "ata.h"
#include "fat12.h"

#if defined(__linux__)
#error "you should compile it with a cross compiler"
#endif


#if defined(__darwin__)
#error "you should use a corss compiler"
#endif


#if !defined(__i386__)
#error "This needs to be compiled with a ix86-elf compiler"
#endif


extern void user_app();



void kmain(uint32_t magic, uint32_t mbi_addr) {
    
    gdt_init();
    idt_init();


    asm volatile("sti");   // enabling cpu interrupts
    pmm_init((multiboot_info_t *)mbi_addr);  // <-- add this
    paging_init();
    heap_init();

    if (ata_detect()) printf("disk found\n");
    else printf("no disk \n");

    fat12_init();

    keyboard_init();
    init_shell();



    for (;;) asm volatile("hlt"); // halt CPU and wait for interrupts
    
}
