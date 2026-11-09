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

## GDT and TSS — `arch/gdt.c`

**What it does.** Sets up the segment descriptors the CPU needs in protected
mode, plus a Task State Segment (TSS) for future ring changes.

**How it works.** planckOS uses a flat memory model. Every code and data
segment has base 0 and limit 4 GB, so segmentation does nothing and paging
does the real work.

| Index | Selector | Descriptor |
|-------|----------|------------|
| 0 | `0x00` | Null (required) |
| 1 | `0x08` | Kernel code (ring 0) |
| 2 | `0x10` | Kernel data (ring 0) |
| 3 | `0x18` | User code (ring 3, selector `0x1B` with RPL 3) |
| 4 | `0x20` | User data (ring 3, selector `0x23`) |
| 5 | `0x28` | TSS |

`gdt_flush` (in `gdt.s`) runs `lgdt`, reloads the data segment registers with
`0x10`, and does a far jump to `0x08:.flush` to reload `cs`.

A descriptor's **limit** is the offset of the *last* valid byte, so the TSS
limit is `sizeof(tss) - 1`. The TSS's `esp0`/`ss0` fields tell the CPU which
stack to switch to when an interrupt arrives in ring 3. `esp0` is still 0
(marked TODO), which is harmless only while everything runs in ring 0.

**Known limits.** The TSS is described but not loaded yet (`tss_flush` /
`ltr` isn't called). That's the first step toward user mode.
