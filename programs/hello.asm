; hello.asm - the smallest planckOS assembly program: print a line, return.
;
; In a Terminal:  asm HELLO.ASM HELLO.ELF   then   exec HELLO.ELF
; (On a disk prepared by 'make programs', HELLO.ELF is the C version; any
; output name works, e.g. asm HELLO.ASM HI.ELF.)

SYS_PRINT equ 0             ; int 0x80 call numbers, see src/arch/syscall.h

_start:
    mov eax, SYS_PRINT
    mov ebx, message        ; the address of the text
    int 0x80
    ret                     ; back to the shell, exit code = eax

message: db "Hello from assembly!", 10, 0
