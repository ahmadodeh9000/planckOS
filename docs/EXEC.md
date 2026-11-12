# Running programs (ELF)

Files: `src/exec/elf.c`, `src/exec/elf.h`.

## What it does

Loads a program stored as a 32-bit ELF executable into memory, so the
kernel can jump to it.

## The program area

Programs don't get their own address space yet. Instead, physical memory
from **32 MB to 48 MB** (`PROGRAM_BASE` to `PROGRAM_END`) is set aside for
them:

```
0x00100000  kernel image
            kernel heap (16 MB)
            free pages
0x02000000  +------------------------+  PROGRAM_BASE (32 MB)
            | program area (16 MB)   |  programs are linked to run here
0x03000000  +------------------------+  PROGRAM_END (48 MB)
            free pages
0x04000000  end of the identity map (64 MB)
```

`elf_init()` (called from `memory_init` in `kmain`, right after
`heap_init`) reserves that range in the PMM, so `pmm_alloc_page()` can never
hand it out. The range is already identity-mapped, so a program linked at
`0x2000000` runs at physical address `0x2000000`. If the machine has less
than 48 MB of RAM, `elf_init` logs it and every load fails.

## How ELF loading works

An ELF file starts with a header (`elf32_header_t`), followed somewhere
by a table of *program headers* (`elf32_phdr_t`). Each program header
describes one *segment*: a range of file bytes and where they go in
memory. Only segments of type `PT_LOAD` matter to a loader.

```
file:   [ ELF header | program headers | .text .rodata .data ... ]
                         |
                         | PT_LOAD: offset, filesz -> vaddr, memsz
                         v
memory: 0x2000000 [ code + data (filesz bytes) | zeros (memsz - filesz) ]
```

`memsz` can be larger than `filesz`. The extra bytes are the program's
`.bss` (zero-initialized globals), which take no space in the file, so the
loader zeroes them.

`elf_load(data, size)` does this in three steps:

1. **`elf_check`** validates the header, in this order, and logs the first
   check that fails to serial:
   - the file is at least as big as the header,
   - magic bytes `7F 'E' 'L' 'F'`,
   - 32-bit (`ident[4] == 1`) and little-endian (`ident[5] == 1`),
   - an executable (`type == 2`) for the i386 (`machine == 3`),
   - `phentsize` equals `sizeof(elf32_phdr_t)`,
   - the whole program header table (`phoff`, `phnum * phentsize`) lies
     inside the file.
2. **`elf_load_segment`** checks each `PT_LOAD` segment: `vaddr .. vaddr +
   memsz` must be inside the program area, `filesz <= memsz`, and
   `offset .. offset + filesz` must be inside the file. It then copies
   `filesz` bytes and zeroes the rest.
3. The **entry point** must be inside the program area too. It's returned,
   or 0 if anything failed.

### Why the checks are written the way they are

Everything in the file comes from the disk and could be corrupted or
deliberately wrong. A naive check like `offset + filesz <= size` can be
fooled: with `offset = 0xFFFFFFF0` and `filesz = 0x20`, the 32-bit sum
wraps around to `0x10`, which passes, and the copy then reads far outside
the buffer. `range_fits(start, length, limit)` checks `start <= limit` and
`length <= limit - start` instead. Once the first part holds, the
subtraction can't wrap, so there is no way to sneak past it.

## Key functions

| Function | Purpose |
|----------|---------|
| `elf_init()` | Reserve the program area in the PMM. |
| `elf_load(data, size)` | Validate and load an ELF file from memory; returns the entry point or 0. |

## Known limits

- Only statically linked, non-relocatable executables (`ET_EXEC`) linked
  at an address inside 32-48 MB.
- One program at a time: every program uses the same area.
