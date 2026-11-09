#include "drivers/vga.h"
#include "drivers/serial.h"
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

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

void kmain(uint32_t magic, uint32_t mbi_addr) {
    serial_init();
    serial_printf("\n%s %s (kernel %s) booting\n", OS_NAME, OS_VERSION, KERNEL_NAME);
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        serial_printf("boot: bad multiboot magic %x\n", magic);
    }

    gdt_init();
    serial_printf("gdt: loaded\n");
    idt_init();
    serial_printf("idt: loaded, PIC remapped\n");

    asm volatile("sti");   // enabling cpu interrupts
    pmm_init((multiboot_info_t *)mbi_addr);
    paging_init();
    heap_init();

    bool has_disk = ata_detect();
    serial_printf("ata: primary master %s\n", has_disk ? "found" : "missing");
    if (has_disk) printf("disk found\n");
    else printf("no disk \n");

    fat12_init();

    keyboard_init();
    serial_printf("keyboard: ready, starting shell\n");
    init_shell();

    for (;;) asm volatile("hlt"); // halt CPU and wait for interrupts
}
