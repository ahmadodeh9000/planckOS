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
planckOS 2.0.0 (kernel Iris) booting
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

The handler does `ticks++` and, once every `TIMER_HZ` ticks, pushes an
`EVENT_TIMER` so the GUI can refresh the taskbar clock and any window that
has an `on_timer`. Keep IRQ handlers this small:
they run with interrupts off. `ticks` is `volatile` because `sleep_ms()` polls it in
a loop while an interrupt changes it underneath.

**Key functions.**

| Function | Purpose |
|----------|---------|
| `timer_install()` | Program the PIT and hook IRQ0. Called from `kmain` before `sti`. |
| `timer_ticks()` | Ticks since boot. Uptime in seconds = `timer_ticks() / TIMER_HZ`. |
| `sleep_ms(ms)` | Halt until at least `ms` milliseconds have passed (10 ms resolution). |
| `timer_format_uptime(buf)` | Uptime as `"HH:MM:SS"` (used by About). |

**Extending it.** A scheduler would hook in here: every N ticks, switch to
the next task.

**Known limits.** 10 ms resolution. The 32-bit counter wraps after about 497
days. `sleep_ms` needs interrupts enabled.

## Real-time clock (RTC) — `drivers/rtc.c`

**What it does.** Reads the wall-clock date and time from the CMOS RTC,
the battery-backed chip that keeps time while the machine is off.

**How it works.** The CMOS chip has a small set of registers behind two
I/O ports: write a register number to `0x70`, then read its value from
`0x71`.

| Register | Holds |
|----------|-------|
| `0x00` / `0x02` / `0x04` | seconds / minutes / hours |
| `0x07` / `0x08` / `0x09` | day / month / year (two digits) |
| `0x0A` (status A) | bit 7: an update is in progress |
| `0x0B` (status B) | bit 1: 24-hour mode; bit 2: binary instead of BCD |

Two quirks shape `rtc_read()`:

- *Torn reads.* The chip updates its registers once a second, and reading
  in the middle of an update can mix old and new values. `rtc_read_raw()`
  waits until status A says no update is in progress, then reads every
  register. `rtc_read()` repeats that until two reads in a row match.
- *Formats.* Values are usually BCD (`0x45` means 45: one decimal digit per
  4-bit nibble), and the hour may be 12-hour with bit 7 as the PM flag.
  `rtc_normalize()` uses status B to turn everything into plain binary
  24-hour values, and adds 2000 to the two-digit year.

Under QEMU the RTC starts from the host's clock. The Makefile passes
`-rtc base=localtime`, so it shows your local time instead of UTC.

**Key functions.**

| Function | Purpose |
|----------|---------|
| `rtc_read(&t)` | Fill an `rtc_time_t` (second, minute, hour, day, month, year). Slow port I/O: never call it from an IRQ handler. |
| `rtc_format_clock(&t, buf)` | `"HH:MM"` (`RTC_CLOCK_LEN` + 1 bytes), used by the taskbar. |
| `rtc_format_day(&t, buf)` | `"DD Mon YYYY"`, e.g. `"09 Nov 2026"` (`RTC_DAY_LEN` + 1 bytes), used by the taskbar. |
| `rtc_format_date(&t, buf)` | `"YYYY-MM-DD HH:MM:SS"` (`RTC_DATE_LEN` + 1 bytes), used by the shell's `date`. |

**Extending it.** Register `0x32` holds the century on most PCs (the ACPI
FADT says where), which would replace the fixed 2000. Setting the clock
means writing the same registers, with bit 7 of status B set while you
write. The RTC can also raise IRQ 8 periodically or at an alarm time
(status B bits 6 and 5).

**Known limits.** Read-only. Assumes the 21st century. No time zones: it
shows whatever the chip holds.

## VGA text console — `drivers/vga.c`

**What it does.** Shows text on the 80x25 text screen. It's a *console
sink*: `vga_console()` returns a `console_t` (see `lib/console.h`), and
everything prints through that. Printing goes through `console_print`,
`console_printf` and `console_print_rainbow`, or plain `printf`, which uses
the default console set in `kmain`.

**How it works.** The VGA card maps its text buffer at physical `0xB8000`.
Each of the 2000 cells is a 16-bit value: the character in the low byte and
an attribute in the high byte (`background << 4 | foreground`). Console
colors are the standard 16 VGA palette indexes, so they go straight into
the attribute. When the cursor passes the last row, `vga_scroll()` moves
every row up by one and clears the bottom row. `'\b'` steps back one cell
(wrapping to the previous row) and blanks it.

The blinking hardware cursor is set through the CRT controller. You write a
register number to port `0x3D4` (`0x0E` = cursor position high byte,
`0x0F` = low byte), then its value to port `0x3D5`.

**Key functions.** `vga_console()`.

**Extending it.** Background colors only need the attribute's high nibble.
`console_t.put_char` could take a combined attribute instead of a
foreground index.

**Known limits.** Black background only. No scrollback.

## Console sinks — `lib/console.c`

A `console_t` is anything that can show a character (`put_char(con, c,
color)`) and clear itself (`clear(con)`), plus a `data` pointer for its own
state. The helpers on top (`console_print`, `console_printf` with
`%s %c %d %u %x`, `console_print_rainbow`) only ever call those two
functions. That's how the shell runs unchanged on the VGA screen and inside
Terminal windows: each provides its own sink.

To make a new sink, fill in a `console_t` with your two functions and point
`data` at your state (see `apps/terminal.c`). A serial-port sink, for
example, would let you run a shell over COM1.

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

**Extending it.** Arrow keys send a `0xE0` prefix byte, then their code
(`0x48` up, `0x50` down, `0x4B` left, `0x4D` right). Remember that a
prefix was seen, and push a new event field (e.g. a `KEY_LEFT` constant
outside the ASCII range). Ctrl and Caps Lock work like Shift: track their
press and release codes.

**Known limits.** US layout only. No Ctrl/Alt/Caps Lock. Extended keys
(arrows, `0xE0`-prefixed codes) are ignored.

## Mouse — `drivers/mouse.c`

**What it does.** Reads the PS/2 mouse and turns its packets into
`EVENT_MOUSE_MOVE`, `EVENT_MOUSE_DOWN` and `EVENT_MOUSE_UP` events, with the
pointer position in screen pixels.

**How it works.** The mouse sits on the second ("auxiliary") port of the
same 8042 controller as the keyboard. Both share data port `0x60`. The
controller's status/command port is `0x64`.

*Setup (`mouse_init`)*, done with interrupts off so no handler eats the
replies:

1. Command `0xA8` enables the aux port.
2. Read the controller configuration byte (`0x20`), set bit 1 (raise IRQ12
   for mouse data), clear bit 5 (aux clock disabled), write it back (`0x60`).
3. Send the mouse "set defaults" (`0xF6`) then "enable reporting" (`0xF4`).
   Each byte for the mouse goes through command `0xD4` ("next data byte is
   for the aux device"), and the mouse answers `0xFA` (ACK).
4. Let the IRQ12 that those replies raised fire *before* installing the
   handler. Otherwise the handler reads a stale byte and gets out of step.

Before every write we wait until status bit 1 (input buffer full) is clear.
Before every read we wait until bit 0 (output buffer full) is set. Both
waits have a timeout, so a missing mouse can't hang the boot.

*Packets.* Each movement or button change sends 3 bytes, one per IRQ12:

| Byte | Bits |
|------|------|
| 0 | 0-2 left/right/middle, 3 always 1, 4 X sign, 5 Y sign, 6-7 overflow |
| 1 | X movement, low 8 bits |
| 2 | Y movement, low 8 bits (positive = **up**) |

The movement values are 9-bit two's complement numbers whose sign bit
lives in byte 0, so `dx = byte1 - (x_sign ? 256 : 0)`. The handler collects
3 bytes, and if a "first" byte doesn't have bit 3 set it drops bytes until
it's back in step. It then updates the position (flipping Y, because screen
Y grows downwards), clamps it to the screen, and pushes events. Packets with
the overflow bits set are ignored.

**Key functions.** `mouse_init(width, height)`. The pointer starts in the
middle of the screen.

**Extending it.** For a scroll wheel, send the "magic" sample-rate
sequence 200, 100, 80 (command `0xF3` each time). Then ask for the device
ID (`0xF2`): if it answers 3, packets become 4 bytes, and the 4th is the
wheel movement.

**Known limits.** No scroll wheel (that needs the IntelliMouse 4-byte
mode). Movement isn't accelerated. In a QEMU window the mouse is relative,
so click into the window to grab it (Ctrl+Alt+G releases it).

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

**Extending it.** The primary master uses select values `0xA0`/`0xE0`.
Supporting several disks means passing the drive (and bus base port) to
each call. Using IRQ14 instead of polling would let the CPU do other work
during transfers.

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

**Key functions.** Nothing in `fat12.c` prints. Every call returns a
`fat12_status` (`FAT12_OK`, `FAT12_NOT_FOUND`, `FAT12_DIR_FULL`,
`FAT12_DISK_FULL`, `FAT12_NO_DISK`), and the caller decides what to show,
for example with `fat12_status_message()`. That lets the text shell and
the GUI apps share the driver.

| Function | Purpose |
|----------|---------|
| `fat12_init()` | Read and sanity-check the BPB, then load the FAT. Without a valid volume every other call returns `FAT12_NO_DISK`. |
| `fat12_list(files, max)` | Fill an array of `{ name, size }` with the root directory's files; returns the count. |
| `fat12_read(name, buf, capacity, &bytes_read)` | Read up to `capacity` bytes. |
| `fat12_write(name, data, size)` | Create or overwrite. |
| `fat12_delete(name)` | Free the chain and mark the entry `0xE5`. |
| `fat12_status_message(status)` | A short English description. |

**Extending it.** Subdirectories are directory entries with attribute `0x10`
whose cluster chain holds more 32-byte entries. Supporting them means
running `root_dir_find` over a chain instead of the fixed root area.

**Known limits.** Root directory only. Only the first FAT copy is updated.
No timestamps.

## Power — `drivers/power.c`

**What it does.** `power_off()` turns the (virtual) machine off. It's used
by the shell's `quit` and the launcher's "Shut down".

**How it works.** Real hardware is switched off through ACPI: you find the
PM1a control port in the ACPI tables and write "sleep type 5 + enable" to
it. QEMU and Bochs expose that register at fixed ports, `0x604` and
`0xB004`, so the driver writes `0x2000` to both. If neither works, it
disables interrupts and halts.

**Extending it.** Real ACPI shutdown means finding the RSDP (search for
`"RSD PTR "` below 1 MB), following it to the FADT for the PM1a port, and
reading the `\_S5` sleep type from the DSDT. A reboot can be done by
pulsing the keyboard controller's reset line (write `0xFE` to port `0x64`).

**Known limits.** Only works in emulators.
