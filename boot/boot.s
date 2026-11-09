; boot.s - Multiboot header and the kernel entry point.
;
; GRUB finds the header below, loads the kernel at 1 MB and jumps to
; 'start' in 32-bit protected mode with paging off. eax holds the
; Multiboot magic and ebx the address of the multiboot_info_t structure.
; We set up a stack and pass both to kmain (see docs/BOOT.md).

bits 32

MULTIBOOT_MAGIC     equ 0x1BADB002
MULTIBOOT_MEMINFO   equ 1 << 1                  ; ask for the memory map
BOOT_FLAGS          equ MULTIBOOT_MEMINFO
CHECKSUM            equ -(MULTIBOOT_MAGIC + BOOT_FLAGS)  ; the three must sum to 0

KERNEL_STACK_SIZE   equ 8192

; link.ld keeps this section at the very start of the image, because GRUB
; only searches the first 8 KB for the header.
section .multiboot
    align 4
    dd MULTIBOOT_MAGIC
    dd BOOT_FLAGS
    dd CHECKSUM

section .text
global start
extern kmain

start:
    cli
    mov esp, stack_top
    push ebx                    ; kmain(magic, mbi_addr): args pushed right to left
    push eax
    call kmain

    ; kmain never returns, but if it did, stop here for good.
.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb KERNEL_STACK_SIZE
stack_top:                      ; the stack grows down from here
