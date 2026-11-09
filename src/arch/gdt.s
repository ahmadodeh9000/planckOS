; gdt.s - load the GDT and reload the segment registers.

KERNEL_CODE_SELECTOR equ 0x08
KERNEL_DATA_SELECTOR equ 0x10

global gdt_flush

; void gdt_flush(gdt_pointer* ptr)
gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]

    mov ax, KERNEL_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; cs can't be loaded with mov; a far jump reloads it.
    jmp KERNEL_CODE_SELECTOR:.flush
.flush:
    ret
