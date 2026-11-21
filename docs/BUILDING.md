# Building and running planckOS

## What you need

| Tool | macOS (Homebrew) | Debian/Ubuntu |
|------|------------------|---------------|
| C compiler | `i686-elf-gcc` | `i686-elf-gcc` or `gcc` + `gcc-multilib` |
| Assembler | `nasm` | `nasm` |
| ISO builder | `i686-elf-grub` (gives `i686-elf-grub-mkrescue`) + `xorriso` | `grub-pc-bin`, `grub-common`, `xorriso`, `mtools` |
| Emulator | `qemu` | `qemu-system-x86` |
| FAT tools (for `make disk`) | built in (`hdiutil`, `newfs_msdos`) | `dosfstools` (`mkfs.fat`) |
| Disk copy (for `make run`, `make programs`, `make disk-models`) | `mtools` | `mtools` |
| Download (for `make models`) | `curl`, `shasum` (preinstalled) | `curl`, `sha256sum` (preinstalled) |
| Python 3 (only for `make chat-model`) | `python3` (Homebrew or python.org) | `python3`, `python3-venv` |
| GitHub CLI (only for `make publish-model`) | `gh` | `gh` |
| Host C compiler (for `make asm-test`, `make higgson-host`) | `cc` (Xcode command line tools) | `gcc` or `cc` |

On Windows, install [WSL2](https://learn.microsoft.com/windows/wsl/install)
with Ubuntu and follow the Debian/Ubuntu column inside it.

`make run` checks for all of these before it does anything
(`scripts/check_tools.sh`) and prints the exact `brew install` or `apt
install` command for whatever is missing.

The Makefile uses `i686-elf-gcc` when it is on your `PATH`. Otherwise it falls
back to the host `gcc` with `-m32 -fno-pic -fno-pie`. Either way, `kernel.c`
refuses to compile unless the target is 32-bit x86 (`__i386__`).

## Make targets

| Target | What it does |
|--------|--------------|
| `make` / `make all` / `make iso` | Builds `build/planckOS.iso`. Never starts QEMU. |
| `make run` | Everything, in order: checks the tools, builds the ISO, creates `disk.img` if it's missing, builds the example programs, runs `make models`, copies onto the disk what changed (models and programs, never over your own files), and boots it in a QEMU window with `disk.img` as the second IDE disk. Serial output (COM1) goes to your terminal. The RTC starts at your local time (`-rtc base=localtime`). |
| `make run-headless` | Same as `make run` but without a window. Handy for reading the serial log. |
| `make check-tools` | Only the tool check that `make run` starts with. |
| `make disk` | Creates a blank 64 MB FAT12 `disk.img`, but only if it doesn't already exist. |
| `make programs` | Builds every `programs/*.c` into `build/programs/*.elf` and copies each onto `disk.img` as `NAME.ELF`, plus the assembly examples `programs/*.asm` as `NAME.ASM` (see `EXEC.md`, `ASSEMBLER.md`). |
| `make models` | Downloads into `models/` whichever model files are missing or don't match `models/MODELS.sha256`: Iris from the GitHub release `iris-v1`, the test model from karpathy/tinyllamas (see `models/README.md`). |
| `make disk-models` | Copies the models onto `disk.img` with 8.3 names (`IRIS.BIN`, `S260K.BIN`, ...), replacing files with those names: for a model you retrained yourself. |
| `make publish-model` | Maintainer only, after `make chat-model`: uploads Iris to the release with `gh` and updates `models/MODELS.sha256` (commit it). |
| `make asm-test` | Builds the built-in assembler for your computer and checks that its bytes for `tests/asm/encoding.asm` are identical to `nasm -O0`'s (see `ASSEMBLER.md`). |
| `make chat-model` | Trains Iris, the chat model, on this computer and writes `models/iris.bin` and `models/iris_tok.bin` (about 40 minutes; see `TRAINING.md`). |
| `make iris-eval` | Runs Iris's model and tool choice on this computer: prints sample conversations and scores the 50 held-out tool requests. |
| `make higgson-host` | Builds the Higgson compiler for your computer as `build/higgson-host/higgson` (`higgson IN.HGS OUT.ASM`), from the same `src/higgson/` files as the kernel's (see `HIGGSON.md`). |
| `make templates-test` | Fills in every Iris app template with every color, assembles each with the built-in assembler and with NASM, and compares the bytes (see "Tools" in `LLM.md`). |
| `make clean` | Deletes `build/`. |

**How `make run` copies onto the disk.** `scripts/sync_disk.sh` keeps a
stamp per file in `build/disk-sync/<the disk's serial number>/` with the
SHA-256 of what it last copied. A file whose checksum hasn't changed is
skipped without opening the disk, so a second `make run` copies nothing. A
file that's on the disk with different contents than planckOS put there
(you edited `HELLO.ASM`, say) is left alone, with a note. A new `disk.img`
has a new serial number, so it's filled from scratch.

You can attach a different disk with `make run DISK=other.img`. The same
works for `make programs DISK=other.img`, which is handy for trying programs
on a scratch copy of your disk.

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
