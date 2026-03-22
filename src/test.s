
global user_app

user_app:
    mov eax,    0
    mov ebx,    msg
    int 0x80


msg db "Hello World",0x0A,0