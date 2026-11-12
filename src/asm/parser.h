/*
 * parser.h - turns the tokens of one line into a statement.
 *
 * A line is:   [label[:]]  [times <count>]  [instruction | db/dw/dd | equ]
 *
 * Values stay unevaluated expressions here (a list of + and - terms),
 * because labels further down the file aren't known yet. The assembler
 * evaluates them in each pass.
 */
#ifndef asm_parser_h__
#define asm_parser_h__

#include "lib/common.h"
#include "asm/error.h"
#include "asm/lexer.h"
#include "asm/encoder.h"

#define EXPR_MAX_TERMS      8
#define DATA_MAX_ITEMS      32

typedef enum {
    TERM_NUMBER,        /* 42, 'A' */
    TERM_SYMBOL,        /* a label or an equ name */
    TERM_HERE,          /* $: the address of the current line */
} term_kind;

typedef struct {
    bool negative;      /* subtract this term instead of adding it */
    term_kind kind;
    uint32_t value;     /* TERM_NUMBER */
    const char* name;   /* TERM_SYMBOL, points into the line */
    uint32_t length;
} term_t;

typedef struct {
    int count;          /* 0 = empty (a memory operand without displacement) */
    term_t terms[EXPR_MAX_TERMS];
} expr_t;

typedef struct {
    operand_type type;  /* OPERAND_REG / _IMM / _MEM */
    int size;           /* register size, or byte/dword keyword; 0 if none */
    int reg;            /* OPERAND_REG: register number */
    int base;           /* OPERAND_MEM: base register number, or NO_BASE */
    expr_t value;       /* OPERAND_IMM: value; OPERAND_MEM: displacement/address */
} operand_syntax_t;

typedef struct {
    bool is_string;
    const token_t* string;  /* is_string: the TOKEN_STRING */
    expr_t value;           /* otherwise */
} data_item_t;

typedef enum {
    STATEMENT_EMPTY,        /* nothing (or only a label) */
    STATEMENT_INSTRUCTION,
    STATEMENT_DATA,         /* db / dw / dd */
    STATEMENT_EQU,          /* name equ value */
} statement_kind;

typedef struct {
    const token_t* label;   /* label or equ name on this line, or NULL */
    statement_kind kind;
    bool has_times;
    expr_t times;           /* repeat count, if has_times */

    int mnemonic;           /* STATEMENT_INSTRUCTION */
    int operand_count;
    operand_syntax_t operands[2];

    int data_size;          /* STATEMENT_DATA: 1 (db), 2 (dw) or 4 (dd) */
    int item_count;
    data_item_t items[DATA_MAX_ITEMS];

    expr_t equ_value;       /* STATEMENT_EQU */
} statement_t;

/* Parse one line's tokens (ending with TOKEN_END) into 'statement'. */
bool parser_parse(const token_t* tokens, statement_t* statement, asm_error_t* error);

#endif
