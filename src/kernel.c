
#include "drivers/vga.h"
#include "arch/gdt.h"
#include "arch/idt.h"
#include "drivers/keyboard.h"
#include "lib/strings.h"
#include "shell/shell.h"
#include "mm/pmm.h"
#include "mm/paging.h"
#include "mm/kmalloc.h"
#include "arch/syscall.h"
#include "drivers/ata.h"
#include "fs/fat12.h"

#if !defined(__i386__)
#error "This needs to be compiled with a ix86-elf compiler"
#endif





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
