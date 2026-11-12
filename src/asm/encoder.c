/*
 * encoder.c - x86 machine code for the supported instructions.
 *
 * Most x86 instructions are built from the same pieces:
 *
 *   [opcode] [ModR/M] [SIB] [displacement] [immediate]
 *
 * - The opcode picks the operation. Its lowest bit (w) is often the
 *   operand size: 0 = 8-bit, 1 = 32-bit (0x88 mov r/m8, 0x89 mov r/m32).
 * - The ModR/M byte (mod:2 reg:3 rm:3) names the operands. 'reg' is a
 *   register, or for some opcodes an "extension" that selects the
 *   operation (0x81 /5 is sub). 'mod' + 'rm' is a register (mod 11) or
 *   a memory address (see put_modrm).
 * - Immediates and displacements are stored little-endian.
 *
 * Choices always match 'nasm -O0': long immediates, rel32 jumps, the
 * shorter "accumulator" forms for eax/al, and 8-bit displacements when
 * they fit.
 */
#include "asm/encoder.h"

#define REG_EAX         0           /* also al */
#define REG_CL          1
#define REG_ESP         4
#define REG_EBP         5

#define MOD_INDIRECT    0x00        /* [reg], or [disp32] with rm = 101 */
#define MOD_DISP8       0x40        /* [reg + disp8] */
#define MOD_DISP32      0x80        /* [reg + disp32] */
#define MOD_REGISTER    0xC0        /* the operand is a register */
#define RM_DISP32_ONLY  0x05        /* with mod 00: [disp32], no register */
#define RM_SIB          0x04        /* an SIB byte follows (needed for esp) */
#define SIB_ESP_ONLY    0x24        /* scale 1, no index, base esp */

#define OPERAND_SIZE_BIT 0x01       /* the w bit: 0 = 8-bit, 1 = 32-bit */
#define REL32_JUMP_LENGTH  5        /* E9/E8 + rel32 */
#define REL32_JCC_LENGTH   6        /* 0F 8x + rel32 */
#define BYTE_MIN        (-128)
#define BYTE_MAX        255

typedef enum {
    KIND_MOV, KIND_ALU, KIND_TEST, KIND_INCDEC, KIND_GROUP3, KIND_SHIFT,
    KIND_PUSH, KIND_POP, KIND_LEA, KIND_JUMP, KIND_JCC, KIND_SINGLE, KIND_INT,
} mnemonic_kind;

typedef struct {
    const char* name;
    mnemonic_kind kind;
    uint8_t opcode;     /* base opcode (meaning depends on the kind) */
    uint8_t extension;  /* the ModR/M reg field for "/n" forms, or the jcc condition */
} mnemonic_info_t;

static const mnemonic_info_t mnemonics[] = {
    { "mov",  KIND_MOV,    0x88, 0 },
    { "add",  KIND_ALU,    0x00, 0 }, { "or",   KIND_ALU,    0x08, 1 },
    { "and",  KIND_ALU,    0x20, 4 }, { "sub",  KIND_ALU,    0x28, 5 },
    { "xor",  KIND_ALU,    0x30, 6 }, { "cmp",  KIND_ALU,    0x38, 7 },
    { "test", KIND_TEST,   0x84, 0 },
    { "inc",  KIND_INCDEC, 0x40, 0 }, { "dec",  KIND_INCDEC, 0x48, 1 },
    { "not",  KIND_GROUP3, 0xF6, 2 }, { "neg",  KIND_GROUP3, 0xF6, 3 },
    { "mul",  KIND_GROUP3, 0xF6, 4 }, { "div",  KIND_GROUP3, 0xF6, 6 },
    { "shl",  KIND_SHIFT,  0xC0, 4 }, { "shr",  KIND_SHIFT,  0xC0, 5 },
    { "push", KIND_PUSH,   0x50, 6 }, { "pop",  KIND_POP,    0x58, 0 },
    { "lea",  KIND_LEA,    0x8D, 0 },
    { "jmp",  KIND_JUMP,   0xE9, 4 }, { "call", KIND_JUMP,   0xE8, 2 },
    { "je",   KIND_JCC,    0x80, 0x4 }, { "jz",  KIND_JCC,   0x80, 0x4 },
    { "jne",  KIND_JCC,    0x80, 0x5 }, { "jnz", KIND_JCC,   0x80, 0x5 },
    { "jb",   KIND_JCC,    0x80, 0x2 }, { "ja",  KIND_JCC,   0x80, 0x7 },
    { "jl",   KIND_JCC,    0x80, 0xC }, { "jge", KIND_JCC,   0x80, 0xD },
    { "jle",  KIND_JCC,    0x80, 0xE }, { "jg",  KIND_JCC,   0x80, 0xF },
    { "ret",  KIND_SINGLE, 0xC3, 0 }, { "nop",  KIND_SINGLE, 0x90, 0 },
    { "int",  KIND_INT,    0xCD, 0 },
};

#define MNEMONIC_COUNT ((int)(sizeof(mnemonics) / sizeof(mnemonics[0])))

static const register_info_t registers[] = {
    { "eax", 0, 32 }, { "ecx", 1, 32 }, { "edx", 2, 32 }, { "ebx", 3, 32 },
    { "esp", 4, 32 }, { "ebp", 5, 32 }, { "esi", 6, 32 }, { "edi", 7, 32 },
    { "al",  0, 8 },  { "cl",  1, 8 },  { "dl",  2, 8 },  { "bl",  3, 8 },
    { "ah",  4, 8 },  { "ch",  5, 8 },  { "dh",  6, 8 },  { "bh",  7, 8 },
};

#define REGISTER_COUNT (sizeof(registers) / sizeof(registers[0]))

/* Bytes are collected here while an instruction is built. */
typedef struct {
    uint8_t* out;
    int length;
    const mnemonic_info_t* info;
    asm_error_t* error;
} code_t;

/* ---- lookup ------------------------------------------------------- */

/* Case-insensitive: does 'text' (length chars) spell the lowercase 'name'? */
static bool name_equals(const char* name, const char* text, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        char c = text[i];
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
        if (name[i] != c) {
            return false;
        }
    }
    return name[length] == '\0';
}

int encoder_find_mnemonic(const char* name, uint32_t length) {
    for (int i = 0; i < MNEMONIC_COUNT; i++) {
        if (name_equals(mnemonics[i].name, name, length)) {
            return i;
        }
    }
    return -1;
}

const register_info_t* encoder_find_register(const char* name, uint32_t length) {
    for (uint32_t i = 0; i < REGISTER_COUNT; i++) {
        if (name_equals(registers[i].name, name, length)) {
            return &registers[i];
        }
    }
    return NULL;
}

/* ---- emitting bytes ----------------------------------------------- */

static void put8(code_t* c, uint8_t byte) {
    c->out[c->length++] = byte;
}

static void put32(code_t* c, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        put8(c, (value >> (8 * i)) & 0xFF);
    }
}

/* "<mnemonic>: <message>", e.g. "add: value doesn't fit in a byte". */
static bool fail(code_t* c, const char* message) {
    return asm_fail(c->error, c->info->name, message, 0xFFFF);
}

static bool fits_in_byte(uint32_t value) {
    int32_t v = (int32_t)value;
    return v >= BYTE_MIN && v <= BYTE_MAX;
}

/* An 8-bit or 32-bit immediate. */
static bool put_imm(code_t* c, uint32_t value, int size) {
    if (size == 32) {
        put32(c, value);
        return true;
    }
    if (!fits_in_byte(value)) {
        return fail(c, "value doesn't fit in a byte");
    }
    put8(c, value & 0xFF);
    return true;
}

/*
 * ModR/M (+ SIB + displacement) for operand 'rm', with 'reg_field' in the
 * middle 3 bits. Two x86 quirks:
 *   - rm = 100 means "an SIB byte follows", so [esp] needs SIB 0x24.
 *   - mod 00 with rm = 101 means [disp32], so [ebp] must be written as
 *     [ebp + 0] with an 8-bit displacement.
 */
static bool put_modrm(code_t* c, int reg_field, const operand_t* rm) {
    uint8_t reg_bits = (reg_field & 7) << 3;

    if (rm->type == OPERAND_REG) {
        put8(c, MOD_REGISTER | reg_bits | rm->reg);
        return true;
    }
    if (rm->base == NO_BASE) {
        put8(c, MOD_INDIRECT | reg_bits | RM_DISP32_ONLY);
        put32(c, rm->value);
        return true;
    }
    if (!rm->known) {
        return fail(c, "a [reg+offset] offset must be a number or a constant defined above");
    }
    int32_t disp = (int32_t)rm->value;
    uint8_t mod = MOD_DISP32;
    if (disp == 0 && rm->base != REG_EBP) {
        mod = MOD_INDIRECT;
    } else if (disp >= -128 && disp <= 127) {
        mod = MOD_DISP8;
    }
    put8(c, mod | reg_bits | (rm->base == REG_ESP ? RM_SIB : rm->base));
    if (rm->base == REG_ESP) {
        put8(c, SIB_ESP_ONLY);
    }
    if (mod == MOD_DISP8) {
        put8(c, disp & 0xFF);
    } else if (mod == MOD_DISP32) {
        put32(c, disp);
    }
    return true;
}

/* ---- operand checks ----------------------------------------------- */

static bool is_rm(const operand_t* op) {
    return op->type == OPERAND_REG || op->type == OPERAND_MEM;
}

/* The operation size, from whichever operands state one. */
static bool operation_size(code_t* c, const instruction_t* ins, int* size) {
    *size = 0;
    for (int i = 0; i < ins->count; i++) {
        int s = ins->op[i].size;
        if (s && *size && s != *size) {
            return fail(c, "operand sizes don't match");
        }
        if (s) {
            *size = s;
        }
    }
    if (*size == 0) {
        return fail(c, "size not specified (add byte or dword)");
    }
    return true;
}

static bool expect_operands(code_t* c, const instruction_t* ins, int count) {
    if (ins->count != count) {
        return fail(c, count == 0 ? "takes no operands" :
                       count == 1 ? "takes one operand" : "takes two operands");
    }
    return true;
}

static uint8_t w_bit(int size) {
    return size == 32 ? OPERAND_SIZE_BIT : 0;
}

/* ---- instruction groups ------------------------------------------- */

/* mov: 88/89 r/m <- reg, 8A/8B reg <- r/m, B0+r/B8+r reg <- imm,
   C6/C7 /0 r/m <- imm, and A0-A3 for al/eax <-> [address]. */
static bool encode_mov(code_t* c, const instruction_t* ins) {
    const operand_t* dst = &ins->op[0];
    const operand_t* src = &ins->op[1];
    int size;

    if (!expect_operands(c, ins, 2) || !operation_size(c, ins, &size)) {
        return false;
    }
    uint8_t w = w_bit(size);
    if (src->type == OPERAND_REG && dst->type == OPERAND_MEM && dst->base == NO_BASE && src->reg == REG_EAX) {
        put8(c, 0xA2 | w);
        put32(c, dst->value);
    } else if (dst->type == OPERAND_REG && src->type == OPERAND_MEM && src->base == NO_BASE && dst->reg == REG_EAX) {
        put8(c, 0xA0 | w);
        put32(c, src->value);
    } else if (src->type == OPERAND_REG && is_rm(dst)) {
        put8(c, 0x88 | w);
        return put_modrm(c, src->reg, dst);
    } else if (dst->type == OPERAND_REG && src->type == OPERAND_MEM) {
        put8(c, 0x8A | w);
        return put_modrm(c, dst->reg, src);
    } else if (src->type == OPERAND_IMM && dst->type == OPERAND_REG) {
        put8(c, (size == 32 ? 0xB8 : 0xB0) + dst->reg);
        return put_imm(c, src->value, size);
    } else if (src->type == OPERAND_IMM && dst->type == OPERAND_MEM) {
        put8(c, 0xC6 | w);
        return put_modrm(c, 0, dst) && put_imm(c, src->value, size);
    } else {
        return fail(c, "invalid operands");
    }
    return true;
}

/* add/or/and/sub/xor/cmp share one layout, offset by the base opcode:
   base+0/1 r/m <- reg, base+2/3 reg <- r/m, base+4/5 al/eax <- imm,
   and 80/81 /n r/m <- imm. */
static bool encode_alu(code_t* c, const instruction_t* ins) {
    const operand_t* dst = &ins->op[0];
    const operand_t* src = &ins->op[1];
    uint8_t base = c->info->opcode;
    int size;

    if (!expect_operands(c, ins, 2) || !operation_size(c, ins, &size)) {
        return false;
    }
    uint8_t w = w_bit(size);
    if (src->type == OPERAND_REG && is_rm(dst)) {
        put8(c, base | w);
        return put_modrm(c, src->reg, dst);
    }
    if (dst->type == OPERAND_REG && src->type == OPERAND_MEM) {
        put8(c, (base + 2) | w);
        return put_modrm(c, dst->reg, src);
    }
    if (src->type != OPERAND_IMM || !is_rm(dst)) {
        return fail(c, "invalid operands");
    }
    if (dst->type == OPERAND_REG && dst->reg == REG_EAX) {
        put8(c, (base + 4) | w);
        return put_imm(c, src->value, size);
    }
    put8(c, 0x80 | w);
    return put_modrm(c, c->info->extension, dst) && put_imm(c, src->value, size);
}

/* test: 84/85 r/m, reg (either order), A8/A9 al/eax, imm, F6/F7 /0 r/m, imm. */
static bool encode_test(code_t* c, const instruction_t* ins) {
    const operand_t* a = &ins->op[0];
    const operand_t* b = &ins->op[1];
    int size;

    if (!expect_operands(c, ins, 2) || !operation_size(c, ins, &size)) {
        return false;
    }
    uint8_t w = w_bit(size);
    if (b->type == OPERAND_REG && is_rm(a)) {
        put8(c, 0x84 | w);
        return put_modrm(c, b->reg, a);
    }
    if (a->type == OPERAND_REG && b->type == OPERAND_MEM) {
        put8(c, 0x84 | w);
        return put_modrm(c, a->reg, b);
    }
    if (b->type != OPERAND_IMM || !is_rm(a)) {
        return fail(c, "invalid operands");
    }
    if (a->type == OPERAND_REG && a->reg == REG_EAX) {
        put8(c, 0xA8 | w);
    } else {
        put8(c, 0xF6 | w);
        if (!put_modrm(c, 0, a)) {
            return false;
        }
    }
    return put_imm(c, b->value, size);
}

/* inc/dec: 40+r / 48+r for 32-bit registers, otherwise FE/FF /0 or /1. */
static bool encode_incdec(code_t* c, const instruction_t* ins) {
    const operand_t* op = &ins->op[0];
    int size;

    if (!expect_operands(c, ins, 1) || !operation_size(c, ins, &size)) {
        return false;
    }
    if (!is_rm(op)) {
        return fail(c, "invalid operand");
    }
    if (op->type == OPERAND_REG && size == 32) {
        put8(c, c->info->opcode + op->reg);
        return true;
    }
    put8(c, 0xFE | w_bit(size));
    return put_modrm(c, c->info->extension, op);
}

/* not/neg/mul/div: F6/F7 with the operation in the ModR/M reg field.
   mul and div use eax (and edx) implicitly. */
static bool encode_group3(code_t* c, const instruction_t* ins) {
    int size;

    if (!expect_operands(c, ins, 1) || !operation_size(c, ins, &size)) {
        return false;
    }
    if (!is_rm(&ins->op[0])) {
        return fail(c, "invalid operand");
    }
    put8(c, c->info->opcode | w_bit(size));
    return put_modrm(c, c->info->extension, &ins->op[0]);
}

/* shl/shr: D0/D1 shift by 1, D2/D3 shift by cl, C0/C1 shift by imm8. */
static bool encode_shift(code_t* c, const instruction_t* ins) {
    const operand_t* dst = &ins->op[0];
    const operand_t* count = &ins->op[1];

    if (!expect_operands(c, ins, 2)) {
        return false;
    }
    if (!is_rm(dst) || dst->size == 0) {
        return fail(c, dst->size == 0 ? "size not specified (add byte or dword)" : "invalid operands");
    }
    uint8_t w = w_bit(dst->size);
    if (count->type == OPERAND_REG && count->reg == REG_CL && count->size == 8) {
        put8(c, 0xD2 | w);
        return put_modrm(c, c->info->extension, dst);
    }
    if (count->type != OPERAND_IMM) {
        return fail(c, "shift count must be cl or a number");
    }
    if (!count->known) {
        return fail(c, "shift count must be a number or a constant defined above");
    }
    put8(c, (count->value == 1 ? 0xD0 : 0xC0) | w);
    if (!put_modrm(c, c->info->extension, dst)) {
        return false;
    }
    return count->value == 1 || put_imm(c, count->value, 8);
}

/* push: 50+r, 68 imm32, FF /6 [mem]. pop: 58+r, 8F /0 [mem]. */
static bool encode_push_pop(code_t* c, const instruction_t* ins, bool is_push) {
    const operand_t* op = &ins->op[0];

    if (!expect_operands(c, ins, 1)) {
        return false;
    }
    if (op->size == 8) {
        return fail(c, "only works with 32-bit operands");
    }
    if (op->type == OPERAND_REG) {
        put8(c, c->info->opcode + op->reg);
        return true;
    }
    if (op->type == OPERAND_IMM && is_push) {
        put8(c, 0x68);
        put32(c, op->value);
        return true;
    }
    if (op->type != OPERAND_MEM) {
        return fail(c, "invalid operand");
    }
    if (op->size == 0) {
        return fail(c, "size not specified (add dword)");
    }
    put8(c, is_push ? 0xFF : 0x8F);
    return put_modrm(c, c->info->extension, op);
}

/* lea reg32, [mem]: 8D /r, computes the address without reading memory. */
static bool encode_lea(code_t* c, const instruction_t* ins) {
    if (!expect_operands(c, ins, 2)) {
        return false;
    }
    if (ins->op[0].type != OPERAND_REG || ins->op[0].size != 32 || ins->op[1].type != OPERAND_MEM) {
        return fail(c, "needs a 32-bit register and a memory operand");
    }
    put8(c, 0x8D);
    return put_modrm(c, ins->op[0].reg, &ins->op[1]);
}

/* jmp/call label: E9/E8 rel32, counted from the end of the instruction.
   jmp/call reg or [mem]: FF /4 or /2. */
static bool encode_jump(code_t* c, const instruction_t* ins) {
    const operand_t* target = &ins->op[0];

    if (!expect_operands(c, ins, 1)) {
        return false;
    }
    if (target->type == OPERAND_IMM) {
        put8(c, c->info->opcode);
        put32(c, target->value - (ins->address + REL32_JUMP_LENGTH));
        return true;
    }
    if (!is_rm(target) || target->size == 8) {
        return fail(c, "invalid operand");
    }
    put8(c, 0xFF);
    return put_modrm(c, c->info->extension, target);
}

/* jcc label: 0F 80+condition rel32. */
static bool encode_jcc(code_t* c, const instruction_t* ins) {
    if (!expect_operands(c, ins, 1)) {
        return false;
    }
    if (ins->op[0].type != OPERAND_IMM) {
        return fail(c, "needs a label");
    }
    put8(c, 0x0F);
    put8(c, c->info->opcode | c->info->extension);
    put32(c, ins->op[0].value - (ins->address + REL32_JCC_LENGTH));
    return true;
}

/* int imm8: CD ib. */
static bool encode_int(code_t* c, const instruction_t* ins) {
    if (!expect_operands(c, ins, 1)) {
        return false;
    }
    if (ins->op[0].type != OPERAND_IMM) {
        return fail(c, "needs a number");
    }
    put8(c, c->info->opcode);
    return put_imm(c, ins->op[0].value, 8);
}

static bool encode_by_kind(code_t* c, const instruction_t* ins) {
    switch (c->info->kind) {
        case KIND_MOV:    return encode_mov(c, ins);
        case KIND_ALU:    return encode_alu(c, ins);
        case KIND_TEST:   return encode_test(c, ins);
        case KIND_INCDEC: return encode_incdec(c, ins);
        case KIND_GROUP3: return encode_group3(c, ins);
        case KIND_SHIFT:  return encode_shift(c, ins);
        case KIND_PUSH:   return encode_push_pop(c, ins, true);
        case KIND_POP:    return encode_push_pop(c, ins, false);
        case KIND_LEA:    return encode_lea(c, ins);
        case KIND_JUMP:   return encode_jump(c, ins);
        case KIND_JCC:    return encode_jcc(c, ins);
        case KIND_INT:    return encode_int(c, ins);
        case KIND_SINGLE:
            if (!expect_operands(c, ins, 0)) {
                return false;
            }
            put8(c, c->info->opcode);
            return true;
    }
    return fail(c, "unknown instruction");
}

int encoder_encode(const instruction_t* ins, uint8_t* out, asm_error_t* error) {
    if (ins->mnemonic < 0 || ins->mnemonic >= MNEMONIC_COUNT) {
        asm_fail(error, "unknown instruction", NULL, 0);
        return -1;
    }
    code_t c = { out, 0, &mnemonics[ins->mnemonic], error };
    return encode_by_kind(&c, ins) ? c.length : -1;
}
