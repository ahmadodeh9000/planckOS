global idt_load

extern isr_handler

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

%macro ISR_NOERRCODE 1
    global isr%1
    isr%1:
        cli
        push long 0
        push long %1
        jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
    global isr%1
    isr%1:
        cli
        push long %1
        jmp isr_common_stub
%endmacro


%macro IRQ_NOERRCODE 2  
    global irq%1
    irq%1:
        cli
        push long 0
        push long %2
        jmp irq_common_stub
%endmacro

;
; CPU EXCEPTIONS
;

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
ISR_NOERRCODE 128
ISR_NOERRCODE 177


;
;
;   IRQ
;


IRQ_NOERRCODE 0, 32   ; Timer
IRQ_NOERRCODE 1, 33   ; Keyboard
IRQ_NOERRCODE 2, 34   ; Cascade (used internally by PIC)
IRQ_NOERRCODE 3, 35   ; COM2
IRQ_NOERRCODE 4, 36   ; COM1
IRQ_NOERRCODE 5, 37   ; LPT2
IRQ_NOERRCODE 6, 38   ; Floppy
IRQ_NOERRCODE 7, 39   ; LPT1 / Spurious

IRQ_NOERRCODE 8, 40   ; RTC / CMOS
IRQ_NOERRCODE 9, 41   ; ACPI / APIC / unused
IRQ_NOERRCODE 10,42   ; Free for peripherals
IRQ_NOERRCODE 11,43   ; Free
IRQ_NOERRCODE 12,44   ; PS2 Mouse
IRQ_NOERRCODE 13,45   ; FPU / Coprocessor
IRQ_NOERRCODE 14,46   ; Primary IDE
IRQ_NOERRCODE 15,47   ; Secondary IDE

extern isr_handler
isr_common_stub:

    pusha

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call isr_handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds

    popa

    add esp, 8      ; remove int_no and err_code

    sti
    iretd

extern irq_handler
irq_common_stub:

    pusha

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds

    popa

    add esp, 8      ; remove int_no and err_code

    sti
    iretd
