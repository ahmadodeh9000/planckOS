; idt.s - interrupt entry stubs and lidt.
;
; The CPU jumps to one of these stubs when an interrupt fires. Each stub
; pushes a fake error code (when the CPU didn't push a real one) and its
; vector number, so every interrupt reaches the common code with the same
; stack layout. The common code saves the remaining registers, which gives
; the C side a registers_t (see idt.h). After the C handler returns it
; restores everything and returns with iretd.

global idt_load
extern isr_handler
extern irq_handler

KERNEL_DATA_SELECTOR equ 0x10

; void idt_load(idt_pointer* ptr)
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; Exceptions where the CPU does not push an error code.
%macro ISR_NOERRCODE 1
    global isr%1
    isr%1:
        cli
        push dword 0
        push dword %1
        jmp isr_common_stub
%endmacro

; Exceptions where the CPU already pushed an error code.
%macro ISR_ERRCODE 1
    global isr%1
    isr%1:
        cli
        push dword %1
        jmp isr_common_stub
%endmacro

; Hardware IRQ %1, delivered on vector %2.
%macro IRQ 2
    global irq%1
    irq%1:
        cli
        push dword 0
        push dword %2
        jmp irq_common_stub
%endmacro

; Save state, call the C handler %1, restore state.
%macro COMMON_STUB 1
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, KERNEL_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                ; registers_t* argument
    call %1
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popa

    add esp, 8              ; drop vector number and error code
    sti
    iretd
%endmacro

; CPU exceptions. 8, 10-14, 17 and 30 come with an error code.
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30
ISR_NOERRCODE 31

; System call gate (int 0x80)
ISR_NOERRCODE 128

; Hardware IRQs, remapped to vectors 32-47 by pic_remap()
IRQ 0,  32      ; PIT timer
IRQ 1,  33      ; PS/2 keyboard
IRQ 2,  34      ; cascade from the slave PIC
IRQ 3,  35      ; COM2
IRQ 4,  36      ; COM1
IRQ 5,  37      ; LPT2
IRQ 6,  38      ; floppy
IRQ 7,  39      ; LPT1 / spurious
IRQ 8,  40      ; RTC
IRQ 9,  41      ; ACPI / free
IRQ 10, 42      ; free
IRQ 11, 43      ; free
IRQ 12, 44      ; PS/2 mouse
IRQ 13, 45      ; FPU
IRQ 14, 46      ; primary ATA
IRQ 15, 47      ; secondary ATA

isr_common_stub:
    COMMON_STUB isr_handler

irq_common_stub:
    COMMON_STUB irq_handler
