# planckOS

A 32-bit x86 hobby operating system built from scratch in C and x86 assembly.

This is my first OS. I started it to understand what actually happens when a computer boots , one rabbit hole led to another, and eventually I had something I could type into.

---

## What it does

- Boots via GRUB (Multiboot-compliant)
- GDT, IDT, PIC, and PIT setup
- VGA text-mode driver with scrolling
- PS/2 keyboard driver with shift support
- Physical memory manager (bitmap-based, driven by Multiboot memory map)
- x86 paging — identity-mapped first 4MB
- Heap allocator (`kmalloc` / `kfree`) with block splitting and coalescing
- `int 0x80` syscall interface
- ATA PIO disk driver
- FAT12 filesystem — read, write, delete, list
- Interactive shell

## Shell commands

| Command | Description |
|---------|-------------|
| `dir` | List files on disk |
| `cat <file>` | Print file contents |
| `write <file>` | Write to a file |
| `del <file>` | Delete a file |
| `info` | Show the OS info |
| `clear` | Clear the screen | 
| `echo` | Print text on the screen |
| `ls` | List files |
| `help` | List the commands |

---

## Building

You'll need an `i686-elf` cross-compiler toolchain and GRUB installed.

```bash
make
```

## Running

```bash
make run
```

Runs planckOS in QEMU (`qemu-system-i386` required).

---

## Project structure

```
planckOS/
├── boot/          # GRUB bootloader config
├── src/           # Kernel source — drivers, mm, fs, shell
├── link.ld        # Linker script
└── Makefile
```

---

## Status

planckOS v1.0.0 is complete. No userspace or multitasking yet — those are next.

---

## Author

Ahmad ... first-year Computer Engineering student.  
Built this to understand how computers actually work. It was worth it.

> Licensed under GPL-3.0
