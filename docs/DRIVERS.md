# Drivers

Device drivers live in `src/drivers/`. Each driver has a small header that
exposes only what the rest of the kernel needs.

## Serial (COM1) — `drivers/serial.c`

**What it does.** Sends text out of the first serial port. QEMU connects
COM1 to your terminal (`-serial stdio` in the Makefile), so `make run` and
`make run-headless` show every line the kernel logs. Use it for debugging.
It keeps working when the screen is in graphics mode or the kernel is
half-broken.

**How it works.** COM1 is a 16550 UART at I/O port `0x3F8`.
`serial_init()` sets the line to 38400 baud, 8 data bits, no parity, 1 stop
bit (8N1). It turns the FIFOs on and the UART's own interrupts off. To set
the baud rate you first set the DLAB bit in the line control register. That
temporarily turns the first two registers into the baud-rate divisor
(115200 / baud).

`serial_putc()` waits until the "transmit holding register empty" bit in the
line status register is set, then writes the byte. It never uses interrupts,
so it's safe to call from IRQ and exception handlers. It turns `\n` into
`\r\n` so terminals render new lines properly.

**Key functions.**

| Function | Purpose |
|----------|---------|
| `serial_init()` | Configure COM1. Called first thing in `kmain`. |
| `serial_putc(c)` | Send one character. |
| `serial_printf(fmt, ...)` | Formatted output. Supports `%s %c %d %u %x %%`. |

**Boot log.** Every init step in `kmain` logs one line, for example:

```
planckOS 1.0.0 (kernel Iris) booting
gdt: loaded
idt: loaded, PIC remapped
pmm: ...
paging: enabled
heap: ...
ata: primary slave found
fat12: 2044 clusters of 32768 bytes
```

**Extending it.** Reading input would need the "data ready" bit (LSR bit 0)
or IRQ4. Supporting COM2 means passing the base port (`0x2F8`) as a
parameter.

**Known limits.** Transmit only. Polling blocks for a moment on every
character, which is fine for logging but too slow for bulk data.

## Timer (PIT) — `drivers/timer.c`

**What it does.** Gives the kernel a clock: a tick counter that goes up
`TIMER_HZ` (100) times per second, plus `sleep_ms()`.

**How it works.** The 8253/8254 Programmable Interval Timer has an input
clock of 1.193182 MHz. Channel 0 counts down from a 16-bit divisor and
raises IRQ0 every time it reaches zero. `timer_install()` writes command
`0x36` to port `0x43` (channel 0, low byte then high byte, mode 3). It then
writes the divisor `1193182 / 100` to port `0x40` and installs the IRQ0
handler.

The handler only does `ticks++`. Keep IRQ handlers this small: they run
with interrupts off. `ticks` is `volatile` because `sleep_ms()` polls it in
a loop while an interrupt changes it underneath.

**Key functions.**

| Function | Purpose |
|----------|---------|
| `timer_install()` | Program the PIT and hook IRQ0. Called from `kmain` before `sti`. |
| `timer_ticks()` | Ticks since boot. Uptime in seconds = `timer_ticks() / TIMER_HZ`. |
| `sleep_ms(ms)` | Halt until at least `ms` milliseconds have passed (10 ms resolution). |

**Extending it.** A scheduler would hook in here: every N ticks, switch to
the next task.

**Known limits.** 10 ms resolution. The 32-bit counter wraps after about 497
days. `sleep_ms` needs interrupts enabled.

## VGA text console — `drivers/vga.c`

**What it does.** Prints text on the 80x25 text screen. It's used by
`printf` and the shell, and by the exception handler.

**How it works.** The VGA card maps its text buffer at physical `0xB8000`.
Each of the 2000 cells is a 16-bit value: the character in the low byte and
an attribute in the high byte (`background << 4 | foreground`, see
`enum vga_color`). Writing to the buffer shows the character immediately.
When the cursor passes the last row, `vga_scroll()` moves every row up by
one and clears the bottom row.

The blinking hardware cursor is set through the CRT controller. You write a
register number to port `0x3D4` (`0x0E` = cursor position high byte,
`0x0F` = low byte), then its value to port `0x3D5`.

**Key functions.** `vga_clear`, `putc`, `print`, `print_number`, `print_hex`,
and the `*_rainbow` variants, which give each character of a string its own
color.

**Known limits.** No colors in `printf` apart from the rainbow variants.
Backspace refuses to move into the first two rows (the shell banner).

## Keyboard — `drivers/keyboard.c`

**What it does.** Turns PS/2 key presses into `EVENT_KEY` events (see
`INPUT.md`).

**How it works.** Each press or release makes the keyboard controller raise
IRQ1 with one byte, a *scan code* (set 1), in port `0x60`. Press codes are
below `0x80`, and a key's release code is its press code with bit 7 set.
The handler:

1. tracks Shift with its press/release codes (`0x2A`/`0x36` and `0xAA`/`0xB6`),
2. ignores every other release,
3. turns the code into ASCII through `scancode_map` or `scancode_map_shift`,
4. pushes `{ EVENT_KEY, key }` into the event queue.

It doesn't echo anything. Showing the character is up to whoever reads
the event.

**Key functions.** `keyboard_init()` hooks IRQ1.

**Known limits.** US layout only. No Ctrl/Alt/Caps Lock. Extended keys
(arrows, `0xE0`-prefixed codes) are ignored.

## ATA disk — `drivers/ata.c`

**What it does.** Reads and writes 512-byte sectors on the data disk with
PIO (the CPU moves every word itself).

**How it works.** The primary IDE bus uses ports `0x1F0`-`0x1F7`. The data
disk is the bus's **slave** drive: QEMU's `-drive ...,index=1` (the classic
`-hdb`) lands there. Drive-select value `0xF0` means slave + LBA mode, and
the top 4 bits of the 28-bit LBA go in its low nibble.

A transfer:

1. `ata_start()` writes the drive select, sector count, LBA bytes and the
   command (`0x20` read, `0x30` write).
2. For each sector, wait until BSY clears and DRQ is set, then move 256
   16-bit words through the data port `0x1F0`.
3. After a write, send CACHE FLUSH (`0xE7`) and wait, so the data really
   reaches the disk image.

`ata_detect()` sends IDENTIFY (`0xEC`). A status of 0 means no drive. A
non-zero signature in the LBA mid/high registers means an ATAPI device
(not a disk).

**Key functions.** `ata_detect()`, `ata_read(lba, count, buf)`,
`ata_write(lba, count, buf)`.

**Known limits.** Only the primary slave. LBA28 (128 GB max). Busy-waits,
and never times out if the drive doesn't answer.

## FAT12 file system — `fs/fat12.c`

**What it does.** Lists, reads, writes and deletes files in the root
directory of the FAT12 data disk.

**How it works.** The disk is laid out as:

```
| boot sector (BPB) | FAT #1 | FAT #2 | root directory | data: cluster 2, 3, ... |
```

`fat12_init()` reads the BPB (BIOS Parameter Block) in sector 0 and works
out where each area starts. It also loads the whole first FAT into RAM.

- **Directory entries** are 32 bytes: an 8.3 name, attributes, the first
  cluster and the file size. A first name byte of `0x00` means "no more
  entries" and `0xE5` means "deleted". Attribute `0x0F` marks long-file-name
  pieces, which are skipped.
- **The FAT** has one 12-bit entry per cluster. It holds the next cluster of
  the file, `0x000` (free), or `>= 0xFF8` (end of chain). Two entries share
  three bytes, so entry *N* starts at byte `N + N/2`: even entries use the
  low 12 bits of that 16-bit word and odd entries the high 12 bits.
  `fat12_set()` writes the changed FAT sector straight back to disk (two
  sectors if the entry straddles a boundary).
- **Writing** a file frees its old chain (if it existed), allocates a new
  chain, writes the data cluster by cluster (zero-padding the last one),
  then fills in the directory entry and saves the directory.

**Key functions.**

| Function | Purpose |
|----------|---------|
| `fat12_init()` | Read the BPB and the FAT. |
| `fat12_list()` | Print each file and its size. |
| `fat12_read(name, buf)` | Read a file into `buf`; returns its size, or 0 if not found. |
| `fat12_write(name, data, size)` | Create or overwrite. |
| `fat12_delete(name)` | Free the chain and mark the entry `0xE5`. |

**Extending it.** Subdirectories are directory entries with attribute `0x10`
whose cluster chain holds more 32-byte entries. Supporting them means
running `root_dir_find` over a chain instead of the fixed root area. A
`fat12_size(name)` would let callers size buffers exactly.

**Known limits.** Root directory only. Only the first FAT copy is updated.
No timestamps. `fat12_read` trusts the caller's buffer to be large enough.
