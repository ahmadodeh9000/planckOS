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

; uint32_t program_call(uint32_t entry)
; Calls entry() and returns its eax (the program's exit code).
program_call:
    push ebp                ; the C calling convention says these four
    push ebx                ; must survive a call; a program might not
    push esi                ; respect that, so save them ourselves
    push edi
    mov [saved_esp], esp
    call [esp + 20]         ; entry: 4 saved registers + return address above it
.restore:
    mov esp, [saved_esp]    ; also fixes a program that left its stack unbalanced
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
