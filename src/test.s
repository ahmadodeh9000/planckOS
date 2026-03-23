
section .text
global user_app

user_app:
    mov eax,    0
    mov ebx,    msg
    int 0x80
    ret

section .data
msg db "BIG BROTHER IS WATCHING !",0x0A,0

section .bss
result resd 1