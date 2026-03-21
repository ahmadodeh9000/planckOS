
#include "vga.h"
#include "gdt/gdt.h"
#include "idt/idt.h"
#include "keyboard.h"
#include "strings.h"
#include "shell.h"
#include "pmm.h"
#include "paging.h"
#include "kmalloc.h"

void kmain(uint32_t magic, uint32_t mbi_addr) {
    
    gdt_init();
    idt_init();


    asm volatile("sti");   // enabling cpu interrupts
    pmm_init((multiboot_info_t *)mbi_addr);  // <-- add this
    paging_init();

    heap_init();
    int* a = kmalloc(sizeof(int));
    int* b = kmalloc(sizeof(int));
    int* c = kmalloc(sizeof(int));

    *a = 32;
    *b = 40;
    *c = 34;
    printf("a : %d , %x\n",*a,a);
    printf("b : %d , %x\n",*b,b);
    kfree(a);
    printf("c : %d , %x\n",*c,c);

    
    for (;;) asm volatile("hlt"); // halt CPU and wait for interrupts
    
}
