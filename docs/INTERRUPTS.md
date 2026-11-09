# Interrupts: GDT, IDT, PIC and system calls

The CPU-level setup lives in `src/arch/`.

## System calls — `arch/syscall.c`

**What it does.** Lets code call into the kernel with `int 0x80`, which is
how future user-mode programs will ask for services.

**How it works.** IDT entry 128 (`0x80`) is installed with DPL 3
(`type_attributes = 0xEE`), so ring-3 code is allowed to trigger it. The
assembly stub saves all registers into a `registers_t` and calls
`isr_handler`, which passes vector 128 to `syscall_handler`. Arguments go in
registers:

| Register | Meaning |
|----------|---------|
| `eax` | System call number (in), return value (out) |
| `ebx` | First argument |

| Number | Name | Effect |
|--------|------|--------|
| 0 | `SYS_PRINT` | Print the string at `ebx`. |
| 1 | `SYS_MALLOC` | `eax = kmalloc(ebx)`. |
| 2 | `SYS_FREE` | `kfree(ebx)`. |
| 3 | `SYS_EXIT` | Print "Process ended" and halt. |
| 4 | `SYS_PRINT_HEX` | Print `ebx` as hex. |

Return values work by writing to `regs->eax`. The stub's `popa` loads the
saved registers back, so the caller sees the new `eax`.

**Extending it.** Add a `SYS_` number in `syscall.h` and a `case` in
`syscall_handler`. Once user mode exists, check every pointer argument
before using it.

**Known limits.** Everything still runs in ring 0, so no call is protected
yet. `SYS_EXIT` halts the whole machine.
