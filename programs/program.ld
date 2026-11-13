/*
 * program.ld - links a planckOS program to run in the program area.
 *
 * The kernel copies each loadable segment to the address the program was
 * linked for, so everything must sit inside 32-48 MB. PROGRAM_BASE must
 * match PROGRAM_BASE in src/exec/elf.h.
 */
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

PROGRAM_BASE = 0x2000000;
PAGE_SIZE = 0x1000;

SECTIONS
{
    . = PROGRAM_BASE;

    .text   : { *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }

    /* Writable data starts on a new page, so the linker makes two segments:
       code (read + execute) and data (read + write). */
    . = ALIGN(PAGE_SIZE);
    .data   : { *(.data .data.*) }
    .bss    : { *(COMMON) *(.bss .bss.*) }

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame)
    }
}
