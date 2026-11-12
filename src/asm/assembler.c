/*
 * assembler.c - the two-pass driver: lines -> tokens -> statements -> bytes.
 *
 * Pass 1 reads the whole file to learn where every label is: it encodes
 * each instruction only to find out how many bytes it takes. Pass 2 reads
 * the file again, now knowing every label's address, and writes the real
 * bytes. This works because an instruction's size never depends on the
 * value of a label defined later: jumps always use 32-bit offsets and
 * label immediates are always 32-bit. Where a size does depend on a value
 * (times counts, [reg+offset] offsets, shift counts, equ values), the
 * value must be known when the line is read, which both passes agree on.
 *
 * All state is static, because the kernel stack is small; the assembler
 * is therefore not reentrant.
 */
#include "asm/asm.h"
#include "asm/lexer.h"
#include "asm/parser.h"
#include "asm/encoder.h"
#include "lib/string.h"

#define MAX_SYMBOLS     256
#define SYMBOL_NAME_MAX 32
#define MAX_LINE_LENGTH 256
#define MAX_TIMES       65536
#define START_SYMBOL    "_start"
#define ELF_IDENT_VERSION 6        /* index of the ELF version byte in ident[] */
#define ELF_VERSION_CURRENT 1
#define PHDR_FLAGS_RWX  7           /* readable, writable, executable */
#define PAGE_ALIGN      0x1000

typedef struct {
    char name[SYMBOL_NAME_MAX];
    uint32_t value;
    int pass;               /* last pass that defined it */
} symbol_t;

typedef struct {
    int pass;               /* 1 or 2 */
    uint32_t address;       /* where the next byte goes */
    uint32_t line_address;  /* where the current line started: the value of $ */
    uint8_t* code;          /* where the code goes (after the ELF headers) */
    uint32_t capacity;      /* room for code */
    uint32_t size;          /* code bytes so far */
    asm_error_t error;
} assembler_t;

static assembler_t as;
static symbol_t symbols[MAX_SYMBOLS];
static int symbol_count;
static token_t tokens[LEXER_MAX_TOKENS];
static statement_t statement;

/* ---- symbols ------------------------------------------------------ */

static symbol_t* symbol_find(const char* name, uint32_t length) {
    for (int i = 0; i < symbol_count; i++) {
        if (strlen(symbols[i].name) == length && memcmp(symbols[i].name, name, length) == 0) {
            return &symbols[i];
        }
    }
    return NULL;
}

/* Pass 1 adds the symbol; pass 2 checks that it still has the same value. */
static bool symbol_define(const token_t* name, uint32_t value) {
    symbol_t* symbol = symbol_find(name->text, name->length);

    if (name->length >= SYMBOL_NAME_MAX) {
        return asm_fail(&as.error, "name is too long", name->text, name->length);
    }
    if (as.pass == 1) {
        if (symbol) {
            return asm_fail(&as.error, "defined twice", name->text, name->length);
        }
        if (symbol_count == MAX_SYMBOLS) {
            return asm_fail(&as.error, "too many labels", NULL, 0);
        }
        symbol = &symbols[symbol_count++];
        memcpy(symbol->name, name->text, name->length);
        symbol->name[name->length] = '\0';
    } else if (!symbol || symbol->value != value) {
        return asm_fail(&as.error, "internal error: label moved between passes", name->text, name->length);
    }
    symbol->value = value;
    symbol->pass = as.pass;
    return true;
}

/* ---- expressions -------------------------------------------------- */

/*
 * Add up the terms of 'expr'. '*known' becomes false if a symbol isn't
 * defined above this line in the current pass: in pass 1 its value isn't
 * known yet (0 is used), in pass 2 it is, but the line must still be
 * treated the same way as in pass 1.
 */
static bool evaluate(const expr_t* expr, uint32_t* value, bool* known) {
    *value = 0;
    *known = true;
    for (int i = 0; i < expr->count; i++) {
        const term_t* term = &expr->terms[i];
        uint32_t v = term->value;
        if (term->kind == TERM_HERE) {
            v = as.line_address;
        } else if (term->kind == TERM_SYMBOL) {
            symbol_t* symbol = symbol_find(term->name, term->length);
            if (!symbol && as.pass == 2) {
                return asm_fail(&as.error, "undefined label", term->name, term->length);
            }
            v = symbol ? symbol->value : 0;
            *known = *known && symbol && symbol->pass == as.pass;
        }
        *value = term->negative ? *value - v : *value + v;
    }
    return true;
}

static bool evaluate_constant(const expr_t* expr, uint32_t* value, const char* what) {
    bool known;
    if (!evaluate(expr, value, &known)) {
        return false;
    }
    return known || asm_fail(&as.error, what, "use only numbers and names defined above", 0xFFFF);
}

/* ---- output ------------------------------------------------------- */

static bool emit(const uint8_t* bytes, uint32_t count) {
    if (count > as.capacity - as.size) {
        return asm_fail(&as.error, "program is too big", NULL, 0);
    }
    if (as.pass == 2) {
        memcpy(as.code + as.size, bytes, count);
    }
    as.size += count;
    as.address += count;
    return true;
}

/* Little-endian, 'size' bytes, with a range check for db and dw. */
static bool emit_value(uint32_t value, int size, bool known) {
    int32_t v = (int32_t)value;
    uint8_t bytes[4];

    if (known && size == 1 && (v < -128 || v > 255)) {
        return asm_fail(&as.error, "value doesn't fit in a byte (db)", NULL, 0);
    }
    if (known && size == 2 && (v < -32768 || v > 65535)) {
        return asm_fail(&as.error, "value doesn't fit in a word (dw)", NULL, 0);
    }
    for (int i = 0; i < size; i++) {
        bytes[i] = (value >> (8 * i)) & 0xFF;
    }
    return emit(bytes, size);
}

/* ---- statements --------------------------------------------------- */

static bool emit_data(const statement_t* st) {
    for (int i = 0; i < st->item_count; i++) {
        const data_item_t* item = &st->items[i];
        if (item->is_string) {
            if (!emit((const uint8_t*)item->string->text, item->string->length)) {
                return false;
            }
            continue;
        }
        uint32_t value;
        bool known;
        if (!evaluate(&item->value, &value, &known) || !emit_value(value, st->data_size, known)) {
            return false;
        }
    }
    return true;
}

/* Turn the parsed operand into an encoder operand with its value. */
static bool resolve_operand(const operand_syntax_t* syntax, operand_t* op) {
    op->type = syntax->type;
    op->size = syntax->size;
    op->reg = syntax->reg;
    op->base = syntax->base;
    op->value = 0;
    op->known = true;
    if (syntax->type == OPERAND_REG) {
        return true;
    }
    return evaluate(&syntax->value, &op->value, &op->known);
}

static bool emit_instruction(const statement_t* st) {
    instruction_t ins;
    uint8_t bytes[ENCODER_MAX_LENGTH];

    ins.mnemonic = st->mnemonic;
    ins.count = st->operand_count;
    ins.address = as.address;
    for (int i = 0; i < st->operand_count; i++) {
        if (!resolve_operand(&st->operands[i], &ins.op[i])) {
            return false;
        }
    }
    int length = encoder_encode(&ins, bytes, &as.error);
    return length >= 0 && emit(bytes, length);
}

static bool emit_once(const statement_t* st) {
    if (st->kind == STATEMENT_DATA) {
        return emit_data(st);
    }
    if (st->kind == STATEMENT_INSTRUCTION) {
        return emit_instruction(st);
    }
    return true;
}

static bool run_statement(const statement_t* st) {
    as.line_address = as.address;
    if (st->kind == STATEMENT_EQU) {
        uint32_t value;
        return evaluate_constant(&st->equ_value, &value, "equ value must be known here") &&
               symbol_define(st->label, value);
    }
    if (st->label && !symbol_define(st->label, as.address)) {
        return false;
    }
    if (!st->has_times) {
        return emit_once(st);
    }
    uint32_t count;
    if (!evaluate_constant(&st->times, &count, "times count must be known here")) {
        return false;
    }
    if (count > MAX_TIMES) {
        return asm_fail(&as.error, "times count is too big", NULL, 0);
    }
    for (uint32_t i = 0; i < count; i++) {
        if (!emit_once(st)) {
            return false;
        }
    }
    return true;
}

/* ---- passes ------------------------------------------------------- */

static bool assemble_line(const char* line, uint32_t length) {
    if (length > MAX_LINE_LENGTH) {
        return asm_fail(&as.error, "line is too long", NULL, 0);
    }
    return lexer_tokenize(line, length, tokens, &as.error) &&
           parser_parse(tokens, &statement, &as.error) &&
           run_statement(&statement);
}

/* One pass over the whole source. On error, '*error_line' is set. */
static bool run_pass(int pass, const char* source, uint32_t length, int* error_line) {
    uint32_t pos = 0;
    int line = 0;

    as.pass = pass;
    as.address = ASM_ORIGIN;
    as.size = 0;
    while (pos < length) {
        uint32_t end = pos;
        while (end < length && source[end] != '\n') {
            end++;
        }
        line++;
        if (!assemble_line(source + pos, end - pos)) {
            *error_line = line;
            return false;
        }
        pos = end + 1;
    }
    return true;
}

static void write_elf_headers(uint8_t* out, uint32_t file_size) {
    elf32_header_t* h = (elf32_header_t*)out;
    elf32_phdr_t* ph = (elf32_phdr_t*)(out + sizeof(elf32_header_t));
    uint32_t magic = ELF_MAGIC;
    symbol_t* start = symbol_find(START_SYMBOL, strlen(START_SYMBOL));

    memset(out, 0, ASM_HEADER_SIZE);
    memcpy(h->ident, &magic, sizeof(magic));
    h->ident[ELF_IDENT_CLASS] = ELF_CLASS_32;
    h->ident[ELF_IDENT_DATA] = ELF_DATA_LSB;
    h->ident[ELF_IDENT_VERSION] = ELF_VERSION_CURRENT;
    h->type = ELF_TYPE_EXEC;
    h->machine = ELF_MACHINE_386;
    h->version = ELF_VERSION_CURRENT;
    h->entry = start ? start->value : ASM_ORIGIN;
    h->phoff = sizeof(elf32_header_t);
    h->ehsize = sizeof(elf32_header_t);
    h->phentsize = sizeof(elf32_phdr_t);
    h->phnum = 1;

    /* One segment: the whole file, headers included, loaded at PROGRAM_BASE. */
    ph->type = PT_LOAD;
    ph->vaddr = ph->paddr = PROGRAM_BASE;
    ph->filesz = ph->memsz = file_size;
    ph->flags = PHDR_FLAGS_RWX;
    ph->align = PAGE_ALIGN;
}

asm_result_t asm_assemble(const char* source, uint32_t length, uint8_t* out, uint32_t capacity) {
    asm_result_t result = { false, 0, 0, "" };

    if (capacity < ASM_HEADER_SIZE) {
        strcpy(result.message, "output buffer is too small");
        return result;
    }
    symbol_count = 0;
    as.code = out + ASM_HEADER_SIZE;
    as.capacity = capacity - ASM_HEADER_SIZE;
    as.error.message[0] = '\0';
    if (!run_pass(1, source, length, &result.line) || !run_pass(2, source, length, &result.line)) {
        memcpy(result.message, as.error.message, ASM_ERROR_MAX);
        return result;
    }
    result.size = ASM_HEADER_SIZE + as.size;
    write_elf_headers(out, result.size);
    result.ok = true;
    return result;
}
