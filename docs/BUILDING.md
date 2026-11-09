# Building and running planckOS

## What you need

| Tool | macOS (Homebrew) | Debian/Ubuntu |
|------|------------------|---------------|
| C compiler | `i686-elf-gcc` | `i686-elf-gcc` or `gcc` + `gcc-multilib` |
| Assembler | `nasm` | `nasm` |
| ISO builder | `i686-elf-grub` (gives `i686-elf-grub-mkrescue`) + `xorriso` | `grub-pc-bin`, `grub-common`, `xorriso`, `mtools` |
| Emulator | `qemu` | `qemu-system-x86` |
| FAT tools (for `make disk`) | built in (`hdiutil`, `newfs_msdos`) | `dosfstools` (`mkfs.fat`) |

The Makefile uses `i686-elf-gcc` when it is on your `PATH`. Otherwise it falls
back to the host `gcc` with `-m32 -fno-pic -fno-pie`. Either way, `kernel.c`
refuses to compile unless the target is 32-bit x86 (`__i386__`).

## Make targets

| Target | What it does |
|--------|--------------|
| `make` / `make all` / `make iso` | Builds `build/planckOS.iso`. Never starts QEMU. |
| `make run` | Boots the ISO in a QEMU window with `disk.img` as the second IDE disk. Serial output (COM1) goes to your terminal. The RTC starts at your local time (`-rtc base=localtime`). |
| `make run-headless` | Same as `make run` but without a window. Handy for reading the serial log. |
| `make disk` | Creates a blank 64 MB FAT12 `disk.img`, but only if it doesn't already exist. |
| `make clean` | Deletes `build/`. |

You can attach a different disk with `make run DISK=other.img`.

## How the build works

Every `.c` and `.s` file under `src/`, plus `boot/boot.s`, is compiled into a
matching object under `build/`. For example, `src/kernel.c` becomes
`build/src/kernel.c.o`. GCC also writes a `.d` file next to each object, so
when you edit a header, only the files that include it are rebuilt.

The objects are linked with `link.ld` into `build/kernel.elf`. `libgcc` is
linked in for helpers such as 64-bit division. The kernel and `boot/grub.cfg`
are then copied into `build/iso/` and turned into a bootable ISO with
`grub-mkrescue`.

Compiler flags worth knowing:

- `-ffreestanding -nostdlib -fno-builtin`: there is no C library. Everything
  the kernel uses lives in `src/`.
- `-O2`: optimized build. Any variable that an interrupt handler changes and
  normal code polls must be `volatile`, or the compiler may keep it in a
  register forever.
- `-Wall -Wextra`: the build should produce no warnings.

## Running and debugging

- **Mouse in the QEMU window.** The PS/2 mouse is relative, so QEMU has to
  grab your mouse: click inside the window, and press Ctrl+Alt+G to let go.
- **Serial log.** Everything the kernel logs with `serial_printf` appears
  in the terminal you ran `make run` from. That includes each boot step,
  exceptions and out-of-memory messages. It's the first place to look when
  something goes wrong, especially in graphics mode, where the VGA text
  screen isn't visible.
- **Text mode.** Change `gfxpayload=1024x768x32` to `gfxpayload=text` in
  `boot/grub.cfg` to boot the old text shell instead of the desktop.
- **QEMU monitor.** In the QEMU window, Ctrl+Alt+2 opens the monitor and
  Ctrl+Alt+1 goes back. Useful commands: `info registers` (CPU state),
  `info pic`, `info mem` (page mappings), `screendump file.ppm`,
  `sendkey a`, `mouse_move 10 0`, `mouse_button 1`.
- **Interrupt tracing.** Run QEMU by hand with `-d int -no-reboot` to print
  every interrupt and exception. That's handy for triple faults.
- **GDB.** Start QEMU paused with a GDB server:

  ```sh
  qemu-system-i386 -cdrom build/planckOS.iso -drive file=disk.img,format=raw,index=1,media=disk -serial stdio -s -S
  ```

  Then in another terminal run `gdb build/kernel.elf` (or `i686-elf-gdb`),
  followed by `target remote :1234`, `break kmain` and `continue`. The
  kernel is built with `-g`, so you get source lines and variables.

## Known limits

- `make disk` on macOS formats the image through `hdiutil attach`, which
  briefly shows the image as a disk device.
