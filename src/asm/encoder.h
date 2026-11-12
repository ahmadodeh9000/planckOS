/*
 * encoder.h - turns one x86 instruction into machine code bytes.
 *
 * The encoder knows nothing about text or labels: it gets an instruction
 * whose operands are already evaluated (register numbers, values,
 * addresses) and writes the opcode, ModR/M byte, displacement and
 * immediate. It always picks the same encoding as 'nasm -O0', which is
 * how its output is checked.
 */
#ifndef asm_encoder_h__
#define asm_encoder_h__

#include "lib/common.h"
#include "asm/error.h"

#define ENCODER_MAX_LENGTH  15      /* longest x86 instruction */
#define NO_BASE             (-1)

typedef enum {
    OPERAND_NONE,
    OPERAND_REG,        /* eax, bl ... */
    OPERAND_IMM,        /* 5, 'A', label (its address) */
    OPERAND_MEM,        /* [ebx], [ebx+4], [label] */
} operand_type;

typedef struct {
    operand_type type;
    int size;           /* 8 or 32; 0 if not stated (immediates, [mem] without byte/dword) */
    int reg;            /* OPERAND_REG: register number 0-7 */
    int base;           /* OPERAND_MEM: base register number, or NO_BASE for [address] */
    uint32_t value;     /* OPERAND_IMM: the value; OPERAND_MEM: displacement or address */
    bool known;         /* value doesn't depend on a symbol defined further down */
} operand_t;

typedef struct {
    int mnemonic;       /* index from encoder_find_mnemonic */
    int count;          /* number of operands, 0-2 */
    operand_t op[2];
    uint32_t address;   /* where the instruction will be in memory */
} instruction_t;

typedef struct {
    const char* name;   /* lowercase: "eax", "bl" */
    int number;         /* the 3-bit number in ModR/M and opcode+r forms */
    int size;           /* 8 or 32 */
} register_info_t;

/* Look up a mnemonic or register by name (lowercase, 'length' chars).
   Returns NULL / -1 if it isn't one. */
int encoder_find_mnemonic(const char* name, uint32_t length);
const register_info_t* encoder_find_register(const char* name, uint32_t length);

/* Encode 'ins' into 'out' (ENCODER_MAX_LENGTH bytes). Returns the length,
   or -1 with 'error' set if the operands don't fit the instruction. */
int encoder_encode(const instruction_t* ins, uint8_t* out, asm_error_t* error);

#endif
