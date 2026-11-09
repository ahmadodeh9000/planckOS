# Boot

Files: `boot/boot.s`, `boot/grub.cfg`, `link.ld`, `src/arch/multiboot.h`,
`src/kernel.c`.

## What it does

Gets the CPU from "GRUB just loaded us" to a running desktop (or the text
shell), in a fixed order where each step only depends on the ones before it.

## How it works

1. **BIOS → GRUB.** QEMU boots the ISO. GRUB reads `boot/grub/grub.cfg`
   (`boot/grub.cfg` in the repo). With `timeout=0` it loads `/boot/kernel`
   right away using the Multiboot 1 protocol, and keeps the 1024x768x32
   graphics mode (`gfxpayload`).
2. **GRUB → `start`** (`boot/boot.s`). GRUB finds the Multiboot header,
   loads the kernel's ELF sections at the addresses in `link.ld` (starting at
   1 MB), switches to 32-bit protected mode with paging off and jumps to
   `start`. It leaves two values in registers:
   - `eax` = `0x2BADB002` (proves a Multiboot loader started us)
   - `ebx` = physical address of the `multiboot_info_t` structure
3. **`start` → `kmain`.** `start` points `esp` at the top of an 8 KB stack
   in `.bss`, pushes `ebx` and `eax` as arguments (C arguments are pushed
   right to left) and calls `kmain(magic, mbi_addr)` in `src/kernel.c`.
4. **`kmain`** checks the magic and brings the kernel up, logging one serial
   line per step:

   | Step | What |
   |------|------|
   | `serial_init` | COM1 logging first, so every later step can report. |
   | `console_set_default` | Plain `printf` goes to the VGA text screen. |
   | `cpu_init` | GDT, IDT with remapped PIC, PIT timer, then `sti`. |
   | `memory_init` | PMM from GRUB's memory map, paging, heap. |
   | `storage_init` | ATA detect, and FAT12 if there is a disk. |
   | `keyboard_init` | Key presses become events. |
   | `fb_init` | Graphics if GRUB gave a 32-bit RGB framebuffer: then `mouse_init` and `gui_main_loop()`. Otherwise `shell_run()` on the text screen. |

### The Multiboot header

```
dd MULTIBOOT_MAGIC   ; 0x1BADB002
dd BOOT_FLAGS        ; bits 0-2: page-align modules, memory map, video mode
dd CHECKSUM          ; -(MAGIC + FLAGS), so the three add up to 0
dd 0, 0, 0, 0, 0     ; address fields (unused for ELF kernels)
dd 0, 1024, 768, 32  ; video request: linear graphics, 1024x768x32
```

GRUB only searches the **first 8 KB** of the kernel file for this header. It
lives in its own `.multiboot` section, and `link.ld` places it at the very
start of `.text` with `KEEP(...)`, so the header can never drift past 8 KB
whatever order the object files are linked in. See `GRAPHICS.md` for how the
video request turns into a framebuffer.

### The linker script (`link.ld`)

| Symbol / section | Meaning |
|------------------|---------|
| `. = 1M` | Load and run address. Below 1 MB is BIOS/VGA territory. |
| `kernel_start` | First byte of the kernel image. |
| `.text` | Multiboot header and code. |
| `.rodata` | String literals and `const` tables (like the font). |
| `.data` | Initialized globals. |
| `.bss` | Zeroed globals, including `COMMON` symbols and the boot stack. |
| `kernel_end` | First byte after the kernel, rounded up to 4 KB. |

Every section starts on a 4 KB page boundary. That keeps code (read/execute)
and data (read/write) in separate ELF segments. It also makes `kernel_start`
and `kernel_end` exact page boundaries, so the physical memory manager can
reserve the image as whole pages (see `MEMORY.md`). C code sees these two
symbols as `extern uint8_t kernel_end[]`: only their *addresses* mean
anything.

### Boot information (`arch/multiboot.h`)

`multiboot_info_t` mirrors the structure GRUB fills in. Each group of fields
is only valid when its bit in `flags` is set:

| Flag bit | Fields | Used by |
|----------|--------|---------|
| 6 (`MULTIBOOT_FLAG_MMAP`) | `mmap_addr`, `mmap_length` | `pmm_init` |
| 12 (`MULTIBOOT_FLAG_FRAMEBUFFER`) | `framebuffer_addr`, `_pitch`, `_width`, `_height`, `_bpp`, `_type`, color field positions | `fb_init` |

`kmain` logs the framebuffer GRUB reports. In graphics mode that's type 1
(RGB), 1024x768x32 at `0xFD000000` under QEMU. With `gfxpayload=text`, it's
type 2 (EGA text), 80x25 at `0xB8000`.

## Key functions

| Function | Purpose |
|----------|---------|
| `start` (`boot.s`) | Entry point: stack, then `kmain`. |
| `kmain(magic, mbi_addr)` | Brings every subsystem up in order. |
| `cpu_init`, `memory_init`, `storage_init` | The three groups of init steps (static, in `kernel.c`). |

## Extending it

- To try the text shell, change `gfxpayload=1024x768x32` to
  `gfxpayload=text` in `boot/grub.cfg`.
- A new subsystem gets its `*_init()` call in `kmain` *after* everything it
  depends on (e.g. anything that allocates must come after `heap_init`).
- Loading files with the kernel (an initrd) would use Multiboot modules:
  `module /boot/file` in `grub.cfg`, read through `mods_addr`/`mods_count`.

## Known limits

- The boot stack is a fixed 8 KB (`KERNEL_STACK_SIZE` in `boot.s`).
- The kernel runs where it was loaded (no higher-half mapping).
- Only Multiboot 1. No UEFI boot.
