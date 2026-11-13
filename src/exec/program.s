; program.s - call a loaded program, and jump back out of it.
;
; Programs run in ring 0 on the kernel stack, as an ordinary function call.
; A program normally ends by returning from its entry point. But it can
; also end in the middle of a system call (SYS_EXIT) or a CPU exception,
; deep inside the kernel's interrupt handling, where a normal return isn't
; possible. For that, program_call remembers its stack pointer, and
; program_unwind throws away everything pushed since (the program's frames
; and the interrupt frames) and returns from program_call as if the
; program had returned.

global program_call
global program_unwind

section .bss
saved_esp:  resd 1          ; esp inside program_call, while a program runs

section .text

; uint32_t program_call(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2)
; Calls fn(a0, a1, a2) with the C calling convention and returns its eax.
program_call:
    push ebp
    mov ebp, esp            ; fixed reference point for our arguments:
                            ; [ebp+8]=fn  [ebp+12]=a0  [ebp+16]=a1  [ebp+20]=a2
    push ebx                ; the C calling convention says these must
    push esi                ; survive a call; a program might not respect
    push edi                ; that, so save them ourselves
    mov [saved_esp], esp
    push dword [ebp + 20]   ; arguments go on the stack right to left,
    push dword [ebp + 16]   ; so a0 ends up on top, at [esp+4] once
    push dword [ebp + 12]   ; 'call' has pushed the return address
    call [ebp + 8]
.restore:
    mov esp, [saved_esp]    ; drops the 3 arguments, and also fixes a
                            ; program that left its stack unbalanced
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

; void program_unwind(uint32_t exit_code)
; Only valid while program_call is active. Never returns to its caller:
; program_call returns 'exit_code' instead.
program_unwind:
    mov eax, [esp + 4]
    mov esp, [saved_esp]
    sti                     ; we leave from inside an interrupt handler, which
                            ; runs with interrupts off; turn them back on
    jmp program_call.restore
