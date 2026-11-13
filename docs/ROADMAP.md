# Roadmap

## Done

**Version 1.0 (`v1.0-text`):** text-mode kernel with GDT/IDT/PIC, a
bitmap PMM, paging, a heap, a PS/2 keyboard, ATA PIO, FAT12 and a shell.

**Since then:**

- *Foundation:* a portable Makefile (i686-elf-gcc or gcc -m32, `build/`
  directory, `make run`), a COM1 serial log, a correct linker script,
  PMM/paging/heap bug fixes (kernel and heap reserved, 64 MB identity map,
  16 MB heap), a PIT clock with `sleep_ms`, and the code reorganized into
  subsystem folders.
- *Graphics:* 1024x768x32 framebuffer via GRUB, back buffer, surfaces and
  an 8x16 font.
- *Input:* PS/2 mouse, and an event queue fed by IRQ handlers.
- *Desktop:* window manager (z-order, focus, dragging, close), taskbar with
  a real-time clock (CMOS RTC), launcher menu, widget helpers.
- *Apps:* Terminal (the same shell as text mode, through console sinks),
  Files, Notepad (cursor keys, Home/End, line numbers, scrolling), Clock,
  About, Shut down.
- *Programs:* an ELF loader and the `exec` command run programs from the
  disk in ring 0 (`EXEC.md`), with `programs/hello.c` as an example.
  Programs can open windows with paint, key, mouse and timer callbacks,
  draw rectangles, lines and text, and read the time
  (`PROGRAMS_GUI.md`, examples `click.c` and `click.asm`). A crashing
  callback closes the program's windows instead of the OS.
- *Assembler:* a two-pass x86 assembler inside the OS (`asm` command,
  `ASSEMBLER.md`). Its encodings are checked against NASM, and its ELF output
  runs with `exec`. Write in Notepad, `asm`, `exec`, all without leaving
  planckOS.
- *Docs:* this folder.

## Good next steps

Roughly from easiest to hardest. Each one says where to start.

### Small

- **An editing cursor in text boxes.** Notepad has one now. `widget_textbox`
  could get the same insertion point and arrow keys.
- **Partial redraws.** Keep a "dirty rectangle" in `gui.c` and add
  `fb_present_rect()`, so a mouse move copies a few KB instead of 3 MB.
- **Delete confirmation** in Files, and a "New" button that opens Notepad.
- **More events for program windows.** Mouse release and move, the right
  button, and a timer faster than once a second would each be one more
  `WIN_EVENT_*` and one more `progwin_*` handler.
- **More assembler instructions.** `adc`, `sbb`, `sar`, `movzx`, `jbe`/`jae`
  and friends are mostly one row in `mnemonics[]` each (see "How to add an
  instruction" in `ASSEMBLER.md`).

### Medium

- **FAT12 subdirectories.** A directory is an entry with attribute `0x10`
  whose cluster chain holds more 32-byte entries. Generalize
  `root_dir_load`/`root_dir_find`/`root_dir_save` to work on a cluster
  chain, add path parsing (`DOCS/NOTES.TXT`), and add `mkdir`/`cd` to the
  shell. Also keep the second FAT copy in sync.
- **Minimize and resize windows.** A `hidden` flag that `wm_draw` skips and
  taskbar buttons toggle, and resizing that reallocates `win->surface`.
- **Real ACPI shutdown and reboot** (`drivers/power.c`).
- **Short jumps and `[base + index*scale]` in the assembler.** Short jumps
  need a size that depends on the distance. A common fix is to assume short,
  and when a jump turns out too far, make it long and run pass 1 again until
  nothing changes.

### Large

- **Multitasking.** Give each task its own kernel stack and saved
  registers. On every timer tick (or when a task blocks), save the current
  `esp` and switch to the next task's stack. The IRQ stub's
  `pusha`/`iret` already saves and restores everything, so a context switch
  is "swap stacks inside the timer handler". Start with kernel threads (for
  example, the shell running in its own thread so a slow `cat` doesn't
  freeze the desktop) before user processes. Shared data (the event queue,
  the window list) then needs locking, or interrupts disabled around it.
- **User mode.** Load the TSS (`ltr`) with a valid `esp0`, mark user pages
  `PAGE_USER`, give each process its own page directory, and enter ring 3
  with `iret`. Programs then talk to the kernel only through `int 0x80`
  (`arch/syscall.c`), which must check every pointer they pass. The ELF
  loader already exists (`EXEC.md`): what's missing is giving each program
  its own address space and running it in ring 3 instead of calling it.
- **A user-space GUI protocol.** Once apps are processes, they can't call
  `window_create` directly. They need system calls to create windows, get
  events and hand over their pixel buffers.

## Known rough edges

- The kernel stack is 8 KB. Deep call chains or big local arrays can
  overflow it silently. Put big buffers on the heap, or raise
  `KERNEL_STACK_SIZE` in `boot.s`.
- Exceptions halt with a message on the VGA text screen, which isn't
  visible in graphics mode. Check the serial log (`make run` prints it).
- The heap never gives memory back to the PMM, and fragmentation is only
  reduced by merging with the next free chunk.
