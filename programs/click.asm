; click.asm - click the window to switch the box between red and blue.
SYS_WIN_CREATE     equ 6
SYS_WIN_ON         equ 7
SYS_WIN_RECT       equ 8
SYS_WIN_INVALIDATE equ 10
WIN_EVENT_PAINT    equ 0
WIN_EVENT_MOUSE    equ 2
RED_XOR_BLUE       equ 0x00FF00FF   ; red ^ this = blue, blue ^ this = red

_start:
    mov eax, SYS_WIN_CREATE
    mov ebx, title
    mov ecx, 200
    mov edx, 100
    int 0x80
    mov [win], eax

    mov eax, SYS_WIN_ON
    mov ebx, [win]
    mov ecx, WIN_EVENT_PAINT
    mov edx, paint
    int 0x80

    mov eax, SYS_WIN_ON
    mov ebx, [win]
    mov ecx, WIN_EVENT_MOUSE
    mov edx, click
    int 0x80

    xor eax, eax
    ret

; paint(id): draw the box in the current color.
paint:
    mov eax, SYS_WIN_RECT
    mov ebx, [esp + 4]
    mov ecx, 0x000A000A     ; x = 10, y = 10
    mov edx, 0x00500020     ; w = 80, h = 32
    mov esi, [color]
    int 0x80
    ret

; click(id, x, y): flip the color, then ask for a repaint.
; We don't draw here; paint will run soon and use the new color.
click:
    mov eax, [color]
    xor eax, RED_XOR_BLUE   ; 0xFF0000 <-> 0x0000FF
    mov [color], eax

    mov eax, SYS_WIN_INVALIDATE
    mov ebx, [esp + 4]
    int 0x80
    ret

title: db "Click me", 0
color: dd 0x00FF0000       ; starts red
win:   dd 0