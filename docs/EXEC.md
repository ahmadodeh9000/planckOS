# Running programs (ELF)

Files: `src/exec/elf.c`, `src/exec/program.c`, `src/exec/program.s`, the
`exec` command in `src/shell/shell.c`, `src/arch/syscall.c`, and the
example programs in `programs/`.

## What it does

Runs programs stored on the FAT12 disk as 32-bit ELF executables. Typing
`exec HELLO.ELF` in a Terminal loads the file into memory, calls it, and
shows its output in that Terminal. When the program ends, by returning,
calling `SYS_EXIT` or crashing, you're back at the prompt.

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

## The `exec` command

`cmd_exec` in `shell/shell.c` puts the pieces together:

1. **Read the file.** `shell_read_program` allocates a 1 MB + 1 byte buffer
   and reads the file into it with `fat12_read`. The extra byte tells it
   whether the file was bigger than the 1 MB limit.
2. **Load it.** `elf_load(image, size)` checks the file and copies its
   segments into the program area. The buffer is freed right after,
   because the program now lives in the program area and the buffer isn't
   needed while it runs.
3. **Run it.** `program_run(entry, sh->con)` calls the program and returns
   how it ended.
4. **Report.** A crash prints the exception's name, and a non-zero exit
   code is printed. A normal exit with code 0 prints nothing extra.

| You type | You see |
|----------|---------|
| `exec HELLO.ELF` | The program's output. |
| `exec NOTES.TXT` | `exec: NOTES.TXT is not a valid planckOS program (the serial log says why)` |
| `exec NOPE.ELF` | `exec: file not found: NOPE.ELF` |
| a program that calls `sys_exit(7)` | `exec: ... exited with code 7` |
| a program that runs an invalid instruction | `exec: ... crashed: Invalid Opcode` |

## How a program runs and ends

A program is just a function at its entry point. There are no processes:
`program_call` (in `program.s`) calls it in ring 0, on the kernel's own
stack, like any C function. Its return value is its exit code.

```
cmd_exec -> program_run -> program_call -> entry()      normal return
                                             |
                                             +-> int 0x80 -> isr_handler
                                             |      -> syscall_handler -> SYS_EXIT
                                             +-> CPU exception -> isr_handler
```

Ending through `SYS_EXIT` or an exception is the tricky part: by then the
CPU is several calls and an interrupt frame deep, below the program.
Returning normally would go back *into the program*. So `program_call`
saves its stack pointer before calling the entry point, and
`program_unwind(code)`:

1. puts that saved stack pointer back into `esp`, which throws away the
   program's frames and the interrupt frames below them,
2. turns interrupts back on with `sti` (interrupt handlers run with them
   off, and we leave one without `iret`),
3. pops the registers `program_call` saved and returns `code` from it.

To `program_run` it looks as if the program returned `code`.
`program_exit` (for `SYS_EXIT`) and `program_crash` (for exceptions) both
use it, but only while a program is running. Otherwise they return, and
the old behavior applies (an exception in the kernel itself still halts
the machine). `program_call` also saves and restores `ebp`, `ebx`, `esi`
and `edi`, and resets `esp` even after a normal return, so a program that
breaks the C calling rules can't corrupt the shell.

## Writing and building a program

Programs live in `programs/`, one `.c` file each. The smallest one is
`programs/hello.c`:

```c
#include "planck.h"

int _start(void) {
    sys_print("Hello from HELLO.ELF!\n");
    sys_print("I was loaded at ");
    sys_print_hex((uint32_t)&_start);
    sys_print("and I talk to the kernel through int 0x80.\n");
    return 0;
}
```

- **`_start` is the entry point**, not `main`: there's no C library to call
  `main` for you. Its return value is the exit code.
- **`planck.h`** is the program's whole view of the kernel: `sys_print`,
  `sys_print_hex` and `sys_exit`, each a few lines of inline assembly that
  put the call number in `eax` and the argument in `ebx`, then run
  `int $0x80`. The numbers must match `src/arch/syscall.h`.
- **No libraries.** No `printf` or `malloc`: only what you write yourself
  plus the system calls.

**`programs/program.ld`** links the program at `0x2000000`
(`PROGRAM_BASE`), the start of the program area, and throws away sections
the loader doesn't need. The linker then records `0x2000000` as each
segment's `vaddr` and as the entry point, which is exactly what `elf_load`
checks and uses.

**Build and install:**

```sh
make programs        # build programs/*.c, copy them onto disk.img
make run             # then in a Terminal: exec HELLO.ELF
```

`make programs` compiles each `programs/<name>.c` into
`build/programs/<name>.elf` with the kernel's freestanding flags plus
`-fno-pic` (the code must not expect to be moved), links it with
`program.ld` and `-z max-page-size=0x1000` (otherwise the linker pads the
file to 64 KB boundaries). It then copies it onto the disk image as
`<NAME>.ELF` with `mcopy` from mtools, which writes into a FAT image
without mounting it. Because of FAT12's 8.3 names, `<name>` can be at most
8 characters. Check what the linker produced with
`i686-elf-readelf -hl build/programs/hello.elf`: you should see `EXEC`,
`Intel 80386`, entry `0x2000000` and one `LOAD` segment at `0x02000000`.

To add a program, create `programs/<name>.c` with an `_start` function and
run `make programs` again.

## Key functions

| Function | Purpose |
|----------|---------|
| `elf_init()` | Reserve the program area in the PMM. |
| `elf_load(data, size)` | Validate and load an ELF file from memory; returns the entry point or 0. |
| `program_run(entry, con)` | Call a loaded program with its output going to `con`; returns `{ crashed, exception, exit_code }`. |
| `program_console()` | The running program's console (used by `SYS_PRINT`), or `NULL`. |
| `program_exit(code)` | End the running program (used by `SYS_EXIT`). |
| `program_crash(vector)` | End the running program after a CPU exception (used by `isr_handler`). |

## How it was tested

- `exec HELLO.ELF` in a Terminal window and in the text-mode shell: the
  output appears in the console that ran it.
- A program that calls `sys_exit(7)` returns to the shell with code 7, and
  its later lines never print. A program that runs `ud2` (an invalid
  instruction) is reported as crashed, and the OS keeps running.
- `exec` on a text file, on a missing file, and with no argument fails
  with a message.
- Hand-corrupted copies of `HELLO.ELF`, each breaking one field, are all
  rejected with the right reason in the serial log: a 20-byte file, wrong
  magic, 64-bit class, `phoff = 0xFFFFFFF0`, `phnum = 0xFFFF`, a segment
  offset that wraps past 4 GB, a segment aimed at the kernel at 1 MB,
  `memsz = 0xFFFFFFFF`, `filesz > memsz`, and an entry point inside the
  kernel. `exec HELLO.ELF` still works afterwards.

## Known limits

- **Ring 0, no protection.** A program runs with full kernel privileges.
  It can overwrite any memory, including the kernel, or disable interrupts.
  The checks above protect against *broken files*, not against *malicious
  code*.
- **An infinite loop freezes the OS.** There's no preemption: while a
  program runs, the GUI's main loop doesn't, so nothing is redrawn. Only
  the reset button helps.
- **Crashes are only caught if the CPU notices them.** Writing garbage
  into memory isn't an exception.
- **Shared kernel stack.** Programs run on the kernel's 8 KB boot stack, so
  deep recursion or big local arrays will overflow it.
- One program at a time, all using the same area. Only statically linked,
  non-relocatable executables (`ET_EXEC`) linked inside 32-48 MB. Files up
  to 1 MB.
