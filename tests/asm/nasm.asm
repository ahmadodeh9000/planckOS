; nasm.asm - feeds encoding.asm to NASM the way the planckOS assembler
; sees it: 32-bit code that starts right after the 84 bytes of ELF headers
; at PROGRAM_BASE (0x2000000 + 0x54). Used only by 'make asm-test'.
bits 32
org 0x2000054
%include "encoding.asm"
