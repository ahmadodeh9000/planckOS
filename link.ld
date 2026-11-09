/*
 * link.ld - kernel memory layout.
 *
 * GRUB loads the kernel at 1 MB (physical == virtual, we identity-map).
 * The Multiboot header must sit in the first 8 KB of the file, so it gets
 * its own section that is placed before .text.
 *
 * Every section starts on a 4 KB page boundary, and kernel_end is rounded
 * up to one, so the PMM can reserve the kernel as whole pages and the heap
 * can start right after it.
 */
OUTPUT_FORMAT(elf32-i386)
ENTRY(start)

PAGE_SIZE = 4K;

SECTIONS
{
    . = 1M;
    kernel_start = .;

    .text ALIGN(PAGE_SIZE) : {
        KEEP(*(.multiboot))
        *(.text .text.*)
    }

    .rodata ALIGN(PAGE_SIZE) : {
        *(.rodata .rodata.*)
    }

    .data ALIGN(PAGE_SIZE) : {
        *(.data .data.*)
    }

    .bss ALIGN(PAGE_SIZE) : {
        *(COMMON)
        *(.bss .bss.*)
    }

    . = ALIGN(PAGE_SIZE);
    kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame)
    }
}
