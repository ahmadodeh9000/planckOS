# Architecture

planckOS is a single-address-space, single-tasking 32-bit x86 kernel
(**Iris**) with a small graphical desktop on top. Everything runs in ring 0
in one loop. There are no processes yet: apps are C code inside the kernel
that the window manager calls back.

This page is the map. The other pages in `docs/` go deep on each part.

## Boot sequence

```
BIOS -> GRUB (boot/grub.cfg)
          | Multiboot 1: loads build/kernel.elf at 1 MB, sets 1024x768x32,
          | eax = 0x2BADB002, ebx = &multiboot_info
          v
boot/boot.s  start          set up an 8 KB stack, call kmain(magic, mbi)
          v
src/kernel.c kmain
   serial_init                  COM1 log, used by every later step
   console_set_default(vga)     where plain printf() goes
   cpu_init                     GDT, IDT + PIC remap, PIT at 100 Hz, sti
   memory_init                  PMM bitmap -> paging (64 MB identity) -> 16 MB heap
   storage_init                 ATA detect -> FAT12 (if a disk is there)
   keyboard_init
   fb_init(mbi)?
     yes -> mouse_init -> gui_main_loop()     (never returns)
     no  -> shell_run(vga_console())          (text mode, never returns)
```

Details: `BOOT.md`.

## Memory map (QEMU, 128 MB)

```
0x00000000 +---------------------------+
           | BIOS, VGA text (0xB8000)  |  reserved in the PMM
0x00100000 +---------------------------+  kernel_start
           | .text .rodata .data .bss  |  kernel image (PMM bitmap, page
           |                           |  tables and boot stack live in .bss)
kernel_end +---------------------------+
           | kernel heap, 16 MB        |  kmalloc/kfree, reserved in the PMM
           +---------------------------+
           | free pages                |  pmm_alloc_page()
0x04000000 +---------------------------+  64 MB: end of the identity map
           | free pages (not mapped)   |
0x08000000 +---------------------------+  end of RAM
           ...
0xFD000000 +---------------------------+
           | linear framebuffer, 3 MB  |  mapped by fb_init()
           +---------------------------+
```

Virtual and physical addresses are the same everywhere (identity mapping).
Details: `MEMORY.md`.

## Folder layout

```
boot/           boot.s (Multiboot header, entry), grub.cfg
link.ld         where every section goes in memory
Makefile        build rules, see BUILDING.md
src/
  kernel.c      kmain: brings everything up in order
  arch/         CPU-specific: GDT, IDT/PIC, interrupt stubs, port I/O,
                syscalls, Multiboot structures
  mm/           pmm (physical pages), paging, kmalloc (heap)
  drivers/      serial, timer (PIT), rtc, vga (text), keyboard, mouse, ata, power
  fs/           fat12
  lib/          common types, string/memory helpers, console sinks + printf
  gfx/          surfaces (pixel drawing), framebuffer, font
  gui/          event queue, main loop, window manager, taskbar, menu,
                widgets, cursor, theme
  shell/        the command shell (text mode and Terminal app)
  apps/         terminal, files, notepad, about, and the app list
docs/           these pages
```

Each folder is one subsystem. Its header files expose only what other parts
need, and everything else is `static` inside the `.c` file.

## Subsystem diagram

Arrows point from a part to the parts it uses. Lower layers never call
upward. Data only travels up through the event queue.

```
                +------------------------------------------------+
   apps/        | terminal   files   notepad   about   (apps.c)   |
                +---+-----------+--------+---------+-------------+
                    |           |        |         |
   shell/       shell.c         |        |         |
                    |           |        |         |
   gui/         +---v-----------v--------v---------v----+
                | wm  taskbar  menu  widget  cursor     |
                | gui.c main loop      theme.h          |
                | event.c queue <----------------+      |
                +---+---------------------------+|------+
                    |                           ||
   gfx/         surface  fb  font               || events pushed
                    |                           || from IRQ handlers
   fs/          fat12                           ||
                    |                           ||
   drivers/     ata  vga  serial  power rtc  keyboard mouse timer
                    |                     \______|_____/
   mm/          pmm  paging  kmalloc             |
                                                 |
   arch/        gdt  idt (PIC, IRQ dispatch) ----+  syscall  io.h
   lib/         string  console              (used by everyone)
```

## How control flows at runtime

There is one thread of execution. Interrupts only interrupt it briefly.

1. A device raises an IRQ. The stub in `arch/idt.s` saves the registers
   and calls `irq_handler`, which calls the driver's handler.
2. The driver handler reads the hardware and **pushes an event**
   (`EVENT_KEY`, `EVENT_MOUSE_*`, `EVENT_TIMER`). It never draws.
3. `gui_main_loop` wakes from `hlt`, takes all queued events and dispatches
   them to the launcher menu, the taskbar or the window manager. The window
   manager forwards them to the focused window's `on_key` / `on_mouse`, or
   to every window's `on_timer`.
4. Apps change their state and call `window_invalidate`.
5. If anything changed, the loop composes a new frame (desktop, windows,
   taskbar, menu, cursor) into the back buffer, and `fb_present` copies it
   to the screen. Then it sleeps again.

In text mode, `shell_run` does the same with a much simpler loop: wait for
an event, and if it's a key, give it to the shell.

## Design rules

- **IRQ handlers only record what happened.** Drawing, printing and disk
  access happen in the main loop.
- **No outside libraries.** Everything, down to `memcpy`, is in `src/`.
- **Text output goes through a `console_t`.** That lets the shell run on
  the VGA screen or in a window unchanged.
- **Drivers don't print.** FAT12, for example, returns `fat12_status`
  values, and the caller decides what to show. Diagnostics go to the
  serial log.
