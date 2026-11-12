; loop.asm - print the numbers 1 to 10, one per line.
;
; In a Terminal:  asm LOOP.ASM LOOP.ELF   then   exec LOOP.ELF

SYS_PRINT        equ 0
SYS_PRINT_NUMBER equ 5
LAST             equ 10

_start:
    mov ecx, 1              ; the counter; int 0x80 keeps every register
next:                       ; except eax when a call returns a value
    mov eax, SYS_PRINT_NUMBER
    mov ebx, ecx
    int 0x80
    mov eax, SYS_PRINT
    mov ebx, newline
    int 0x80
    inc ecx
    cmp ecx, LAST
    jle next                ; signed "less or equal": loop while ecx <= 10
    xor eax, eax            ; exit code 0
    ret

newline: db 10, 0
