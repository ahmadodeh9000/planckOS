; encoding.asm - every instruction form the planckOS assembler supports.
;
; 'make asm-test' assembles this file with the planckOS assembler and with
; 'nasm -O0', and checks that both produce exactly the same bytes. It is
; never run, only compared, so the instructions don't need to make sense.
; Conditional jumps say 'strict near' because NASM 3 would otherwise pick
; the short 2-byte form; planckOS always uses the long form.

COUNT equ 5
FLAG  equ COUNT + 3

_start:
    ; ---- mov --------------------------------------------------------
    mov eax, ebx
    mov cl, dh
    mov ecx, 12345
    mov edx, -1
    mov ah, 7
    mov esi, 'A'
    mov edi, message
    mov ebx, message + 4
    mov ecx, COUNT
    mov eax, [ebx]
    mov eax, [ebp]
    mov eax, [esp]
    mov eax, [ebx+4]
    mov eax, [ebx-4]
    mov eax, [ebx+0]
    mov eax, [esi+200]
    mov eax, [esp+8]
    mov eax, [ebp-12]
    mov eax, [table]
    mov al, [table]
    mov [table], eax
    mov [table], al
    mov ebx, [table]
    mov bl, [table+2]
    mov [table+4], ecx
    mov [ebx], ecx
    mov [ebx+4], cl
    mov [esp], eax
    mov byte [ebx], 5
    mov dword [ebx+8], 0x12345678
    mov dword [table], 7
    mov byte [table], 'z'

    ; ---- add or and sub xor cmp -------------------------------------
    add ebx, ecx
    add bl, ch
    add edx, 1000
    add eax, 7
    add al, 7
    add bl, 7
    add esi, -5
    add edi, [ebx+4]
    add [ebx], eax
    add dword [ebx], 100
    add byte [ebx+1], 9

    or ebx, ecx
    or bl, ch
    or edx, 1000
    or eax, 7
    or al, 7
    or bl, 7
    or esi, -5
    or edi, [ebx+4]
    or [ebx], eax
    or dword [ebx], 100
    or byte [ebx+1], 9

    and ebx, ecx
    and bl, ch
    and edx, 1000
    and eax, 7
    and al, 7
    and bl, 7
    and esi, -5
    and edi, [ebx+4]
    and [ebx], eax
    and dword [ebx], 100
    and byte [ebx+1], 9

    sub ebx, ecx
    sub bl, ch
    sub edx, 1000
    sub eax, 7
    sub al, 7
    sub bl, 7
    sub esi, -5
    sub edi, [ebx+4]
    sub [ebx], eax
    sub dword [ebx], 100
    sub byte [ebx+1], 9

    xor ebx, ecx
    xor bl, ch
    xor edx, 1000
    xor eax, 7
    xor al, 7
    xor bl, 7
    xor esi, -5
    xor edi, [ebx+4]
    xor [ebx], eax
    xor dword [ebx], 100
    xor byte [ebx+1], 9

    cmp ebx, ecx
    cmp bl, ch
    cmp edx, 1000
    cmp eax, 7
    cmp al, 7
    cmp bl, 7
    cmp esi, -5
    cmp edi, [ebx+4]
    cmp [ebx], eax
    cmp dword [ebx], 100
    cmp byte [ebx+1], 9

    ; ---- test -------------------------------------------------------
    test ebx, ecx
    test bl, dl
    test eax, 5
    test al, 1
    test ebx, 0x100
    test cl, 2
    test [ebx], eax
    test eax, [ebx]
    test dword [ebx], 3
    test byte [ebx], 3

    ; ---- inc dec ----------------------------------------------------
    inc eax
    inc edi
    inc bl
    inc dword [ebx]
    inc byte [table]
    dec ecx
    dec ah
    dec dword [esp+4]

    ; ---- not neg mul div --------------------------------------------
    not eax
    not bl
    neg ecx
    neg dword [ebx]
    mul ebx
    mul cl
    mul dword [table]
    div ecx
    div byte [ebx]

    ; ---- shl shr ----------------------------------------------------
    shl eax, 1
    shl eax, 3
    shl eax, cl
    shl bl, 1
    shl bl, 4
    shr edx, 1
    shr edx, 31
    shr ecx, cl
    shr dword [ebx], 1
    shr dword [ebx], 2
    shl byte [ebx], cl

    ; ---- push pop lea -----------------------------------------------
    push eax
    push ebp
    push 5
    push message
    push dword [ebx]
    pop ebx
    pop edi
    pop dword [table]
    lea eax, [ebx+8]
    lea esi, [esp+4]
    lea edi, [table]

    ; ---- jumps and calls --------------------------------------------
back:
    jmp back
    jmp forward
    jmp eax
    jmp [ebx]
    call back
    call forward
    call ecx
    call [table]
    je strict near back
    jne strict near back
    jz strict near forward
    jnz strict near forward
    jl strict near back
    jle strict near forward
    jg strict near back
    jge strict near forward
    ja strict near back
    jb strict near forward
forward:
    ret
    nop
    int 0x80
    int 3

    ; ---- data -------------------------------------------------------
message: db "Hello, planckOS!", 10, 0
length equ $ - message
    db 'A', 'B' + 1, -1, 255
    dw 0x1234, -2, length
    dd message, $, 0xDEADBEEF, FLAG
table: times 4 dd 0
    times 3 db 0x90
    times 2 nop
