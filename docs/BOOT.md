# Boot

## What happens

1. **BIOS → GRUB.** QEMU boots the ISO. GRUB reads `boot/grub/grub.cfg`
   (`boot/grub.cfg` in the repo). With `timeout=0` it loads `/boot/kernel`
   right away using the Multiboot 1 protocol.
2. **GRUB → `start`** (`boot/boot.s`). GRUB looks for the Multiboot header,
   loads the kernel's ELF sections at the addresses in `link.ld` (starting at
   1 MB), switches to 32-bit protected mode with paging off and jumps to
   `start`. It leaves two values in registers:
   - `eax` = `0x2BADB002` (proves a Multiboot loader started us)
   - `ebx` = physical address of the `multiboot_info_t` structure
3. **`start` → `kmain`.** `start` points `esp` at the top of an 8 KB stack
   in `.bss`, pushes `ebx` and `eax` as arguments and calls
   `kmain(magic, mbi_addr)` in `src/kernel.c`.
4. **`kmain`** checks the magic, then brings the kernel up in order:
   `serial_init` → `cpu_init` (GDT, IDT/PIC, PIT, `sti`) → `memory_init`
   (PMM, paging, heap) → `storage_init` (ATA detect, FAT12 if a disk is
   present) → keyboard → `shell_run()`. Each step logs one line to serial.

## The Multiboot header

```
dd MULTIBOOT_MAGIC   ; 0x1BADB002
dd BOOT_FLAGS        ; bit 1: give us the memory map
dd CHECKSUM          ; -(MAGIC + FLAGS), so the three add up to 0
```

GRUB only searches the **first 8 KB** of the kernel file for this header. It
lives in its own `.multiboot` section, and `link.ld` places it at the very
start of `.text` with `KEEP(...)`, so the header can never drift past 8 KB
whatever order the object files are linked in.

## The linker script (`link.ld`)

| Symbol / section | Meaning |
|------------------|---------|
| `. = 1M` | Load and run address. Below 1 MB is BIOS/VGA territory. |
| `kernel_start` | First byte of the kernel image. |
| `.text` | Multiboot header and code. |
| `.rodata` | String literals and `const` tables. |
| `.data` | Initialized globals. |
| `.bss` | Zeroed globals, including `COMMON` symbols and the boot stack. |
| `kernel_end` | First byte after the kernel, rounded up to 4 KB. |

Every section starts on a 4 KB page boundary. That keeps code (read/execute)
and data (read/write) in separate ELF segments. It also makes `kernel_start`
and `kernel_end` exact page boundaries, so the physical memory manager can
reserve the image as whole pages (see `MEMORY.md`).

## Known limits

- The boot stack is a fixed 8 KB.
- The kernel runs where it was loaded (no higher-half mapping).
