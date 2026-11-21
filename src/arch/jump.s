; jump.s - setjmp, longjmp and call_on_stack (see jump.h).
;
; A C function may change eax, ecx and edx freely, but must give back
; ebx, esi, edi, ebp and esp as it found them. So that's all setjmp saves,
; plus its own return address. longjmp puts those back and jumps to the
; saved address: to the code that called setjmp, it looks as if setjmp
; had just returned again, with every jumped-over frame gone.

; jmp_buf layout, one dword each (the same order as in jump.h)
JB_EBX  equ 0
JB_ESI  equ 4
JB_EDI  equ 8
JB_EBP  equ 12
JB_ESP  equ 16
JB_EIP  equ 20

global setjmp
global longjmp
global call_on_stack

; int setjmp(jmp_buf env)
setjmp:
    mov eax, [esp + 4]      ; env
    mov [eax + JB_EBX], ebx
    mov [eax + JB_ESI], esi
    mov [eax + JB_EDI], edi
    mov [eax + JB_EBP], ebp
    lea ecx, [esp + 4]      ; esp as the caller will see it after our ret
    mov [eax + JB_ESP], ecx
    mov ecx, [esp]          ; our return address: where to resume
    mov [eax + JB_EIP], ecx
    xor eax, eax            ; the first return gives 0
    ret

; void longjmp(jmp_buf env, int value)
longjmp:
    mov edx, [esp + 4]      ; env
    mov eax, [esp + 8]      ; what setjmp will return
    test eax, eax
    jnz .restore
    inc eax                 ; never 0: that means "first return"
.restore:
    mov ebx, [edx + JB_EBX]
    mov esi, [edx + JB_ESI]
    mov edi, [edx + JB_EDI]
    mov ebp, [edx + JB_EBP]
    mov esp, [edx + JB_ESP] ; drops every frame pushed since setjmp
    jmp [edx + JB_EIP]

; void call_on_stack(void (*fn)(void*), void* arg, void* stack_top)
; Calls fn(arg) with esp at stack_top, then comes back to our own stack.
; ebp marks where our stack was, and fn preserves ebp like any C function.
call_on_stack:
    push ebp
    mov ebp, esp
    mov eax, [ebp + 8]      ; fn
    mov ecx, [ebp + 12]     ; arg
    mov esp, [ebp + 16]     ; from here on, the other stack
    push ecx
    call eax
    mov esp, ebp            ; back to ours
    pop ebp
    ret
