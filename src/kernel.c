
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
#include "fat32.h"

extern void user_app();



void kmain(uint32_t magic, uint32_t mbi_addr) {
    
    gdt_init();
    idt_init();


    asm volatile("sti");   // enabling cpu interrupts
    pmm_init((multiboot_info_t *)mbi_addr);  // <-- add this
    paging_init();
    heap_init();

    if (ata_detect()) printf("disk found\n");
    else printf("no disk niggers\n");

    fat32_init();
    fat32_list_root();

    uint8_t file_buf[512];
    uint32_t size = fat32_read_file("HELLO.TXT", file_buf);
    if (size) {
        file_buf[size] = '\0';
        printf("File contents: %s\n", (char *)file_buf);
    }


    keyboard_init();
    init_shell();

    printf("outside shell \n");

    for (;;) asm volatile("hlt"); // halt CPU and wait for interrupts
    
}
