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
ata: primary master found
fat12: 2044 clusters of 32768 bytes
```

**Extending it.** Reading input would need the "data ready" bit (LSR bit 0)
or IRQ4. Supporting COM2 means passing the base port (`0x2F8`) as a
parameter.

**Known limits.** Transmit only. Polling blocks for a moment on every
character, which is fine for logging but too slow for bulk data.
