bits 32												; since we are working in 32 bits, this will make sure the instructions are 32 bit

MAGIC 			equ 0x1BADB002						; magic number 
BOOT_FLAGS		equ 0x00000002						; boot flags
CHECKSUM		equ -(MAGIC + BOOT_FLAGS)			; checks the validity 

section .text						
	align 4
	dd MAGIC
	dd BOOT_FLAGS
	dd CHECKSUM

									
global start										; main entry
extern kmain										; extern kmain from kernel.c

start:								
	cli	
	mov esp, stack_space
	push ebx										; pushing the multibootinfo
	push eax										; pushing the MAGIC number
	call kmain
	hlt

hlt_kernel:	
	cli
	hlt
	jmp hlt_kernel

section .bss
resb 8192											; reserves 8192 for the stacks 
stack_space:
