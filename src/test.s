
section .text
global user_app

user_app:
    mov eax,    0
    mov ebx,    msg
    int 0x80

    mov eax, 1
    mov ebx, 64
    int 0x80

    mov [result], eax

    mov eax, 4
    mov ebx, [result]
    int 0x80

    mov eax, 3
    int 0x80

    ret

section .data
msg db "Hello From the user",0x0A,0

section .bss
result resd 1