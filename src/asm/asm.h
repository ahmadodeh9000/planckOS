/*
 * asm.h - the planckOS assembler: NASM-style 32-bit x86 source in, a
 * runnable ELF executable out.
 *
 * The output is one PT_LOAD segment loaded at PROGRAM_BASE: the ELF
 * header and program header first, then the code. So the first
 * instruction is at ASM_ORIGIN, and execution starts at the label _start
 * if there is one, or at ASM_ORIGIN otherwise.
 */
#ifndef asm_h__
#define asm_h__

#include "lib/common.h"
#include "asm/error.h"
#include "exec/elf.h"

#define ASM_HEADER_SIZE (sizeof(elf32_header_t) + sizeof(elf32_phdr_t))
#define ASM_ORIGIN      (PROGRAM_BASE + ASM_HEADER_SIZE)

typedef struct {
    bool ok;
    uint32_t size;      /* ok: bytes written to 'out' (the whole ELF file) */
    int line;           /* !ok: line of the error, counted from 1 */
    char message[ASM_ERROR_MAX];
} asm_result_t;

/* Assemble 'length' bytes of 'source' into 'out' (at most 'capacity'
   bytes). Never reads or writes outside those buffers. Not reentrant: it
   keeps its symbol table in static memory. */
asm_result_t asm_assemble(const char* source, uint32_t length, uint8_t* out, uint32_t capacity);

#endif
