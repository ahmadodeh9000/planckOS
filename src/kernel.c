
#include "vga.h"
#include "gdt/gdt.h"
#include "idt/idt.h"
#include "keyboard.h"
#include "strings.h"
#include "shell.h"
#include "pmm.h"

void kmain(uint32_t magic, uint32_t mbi_addr) {
    
    gdt_init();
    idt_init();


    asm volatile("sti");   // enabling cpu interrupts
    pmm_init((multiboot_info_t *)mbi_addr);  // <-- add this
    
    void *p1 = pmm_alloc_page();
    void *p2 = pmm_alloc_page();
    void *p3 = pmm_alloc_page();

    printf("p1 %x\n",p1);
    printf("p2 %x\n",p2);
    printf("p3 %x\n",p3);

    pmm_free_page(p2);

    void *p4 = pmm_alloc_page();
    printf("p4 %x\n",p4);


    
    for (;;) asm volatile("hlt"); // halt CPU and wait for interrupts
    
}
