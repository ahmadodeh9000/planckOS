/*
 * kernel.c - kmain, the C entry point of the Iris kernel.
 *
 * boot/boot.s calls kmain with the Multiboot magic and the address of
 * GRUB's boot information. kmain brings the subsystems up in dependency
 * order (CPU tables, then memory, then devices), logging each step to the
 * serial port, and then hands over to the shell.
 */
#include "arch/gdt.h"
#include "arch/idt.h"
#include "arch/multiboot.h"
#include "mm/pmm.h"
#include "mm/paging.h"
#include "mm/kmalloc.h"
#include "drivers/serial.h"
#include "drivers/vga.h"
#include "drivers/timer.h"
#include "drivers/keyboard.h"
#include "drivers/ata.h"
#include "fs/fat12.h"
#include "gfx/fb.h"
#include "drivers/mouse.h"
#include "gui/gui.h"
#include "shell/shell.h"
#include "exec/elf.h"

#if !defined(__i386__)
#error "planckOS must be built for 32-bit x86 (i686-elf-gcc or gcc -m32)"
#endif

static void cpu_init(void) {
    gdt_init();
    serial_printf("gdt: loaded\n");
    idt_init();
    serial_printf("idt: loaded, PIC remapped\n");
    timer_install();
    serial_printf("timer: PIT at %u Hz\n", TIMER_HZ);
    asm volatile("sti");
}

static void memory_init(multiboot_info_t* mbi) {
    pmm_init(mbi);
    paging_init();
    heap_init();
    elf_init();
}

static void log_framebuffer(multiboot_info_t* mbi) {
    if (!(mbi->flags & MULTIBOOT_FLAG_FRAMEBUFFER)) {
        serial_printf("boot: no framebuffer info\n");
        return;
    }
    serial_printf("boot: framebuffer type %u, %ux%ux%u, pitch %u, at %x\n",
                  mbi->framebuffer_type, mbi->framebuffer_width, mbi->framebuffer_height,
                  mbi->framebuffer_bpp, mbi->framebuffer_pitch, (uint32_t)mbi->framebuffer_addr);
}

static void storage_init(void) {
    bool has_disk = ata_detect();

    serial_printf("ata: primary slave %s\n", has_disk ? "found" : "missing");
    if (has_disk) {
        fat12_init();
    }
}

void kmain(uint32_t magic, uint32_t mbi_addr) {
    serial_init();
    console_set_default(vga_console());
    serial_printf("\n%s %s (kernel %s) booting\n", OS_NAME, OS_VERSION, KERNEL_NAME);
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        serial_printf("boot: bad multiboot magic %x\n", magic);
    }

    multiboot_info_t* mbi = (multiboot_info_t*)mbi_addr;
    log_framebuffer(mbi);

    cpu_init();
    memory_init(mbi);
    storage_init();

    keyboard_init();
    serial_printf("keyboard: ready\n");

    if (fb_init(mbi)) {
        surface_t* screen = fb_back_buffer();
        mouse_init(screen->width, screen->height);
        gui_main_loop();
    }

    /* No usable framebuffer: fall back to the text-mode shell. */
    serial_printf("shell: starting in text mode\n");
    shell_run(vga_console());
}
