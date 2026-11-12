/*
 * parser.c - recognizes labels, directives, instructions and operands.
 *
 * It walks the token array with an index ('pos') and never reads past
 * the TOKEN_END that the lexer always puts last, so any token sequence is
 * safe to feed it: wrong ones end in an error message.
 */
#include "asm/parser.h"

#define MAX_STRING_NUMBER 4     /* 'abcd' is the longest string usable as a number */

typedef struct {
    const token_t* tokens;
    int pos;
    asm_error_t* error;
} parser_t;

static const token_t* current(const parser_t* p) {
    return &p->tokens[p->pos];
}

static bool at(const parser_t* p, token_type type) {
    return current(p)->type == type;
}

static bool fail_at(parser_t* p, const char* message) {
    const token_t* t = current(p);
    if (t->type == TOKEN_END) {
        return asm_fail(p->error, message, "end of line", 11);
    }
    return asm_fail(p->error, message, t->text, t->length);
}

static bool is_register(const token_t* t) {
    return t->type == TOKEN_IDENT && encoder_find_register(t->text, t->length) != NULL;
}

static bool is_mnemonic(const token_t* t) {
    return t->type == TOKEN_IDENT && encoder_find_mnemonic(t->text, t->length) >= 0;
}

static bool is_data_directive(const token_t* t) {
    return token_is(t, "db") || token_is(t, "dw") || token_is(t, "dd");
}

/* Words that start the rest of a statement (so whatever is before them is a label). */
static bool is_keyword(const token_t* t) {
    return is_mnemonic(t) || is_data_directive(t) || token_is(t, "times") || token_is(t, "equ");
}

/* ---- expressions -------------------------------------------------- */

/* A quoted string of 1-4 characters used as a number, e.g. 'A' = 65. */
static bool string_value(parser_t* p, uint32_t* value) {
    const token_t* t = current(p);
    if (t->length == 0 || t->length > MAX_STRING_NUMBER) {
        return fail_at(p, "a string used as a number must have 1-4 characters");
    }
    *value = 0;
    for (uint32_t i = 0; i < t->length; i++) {
        *value |= (uint32_t)(uint8_t)t->text[i] << (8 * i);   /* little-endian, like NASM */
    }
    return true;
}

static bool parse_term(parser_t* p, term_t* term) {
    const token_t* t = current(p);

    if (t->type == TOKEN_NUMBER) {
        term->kind = TERM_NUMBER;
        term->value = t->value;
    } else if (t->type == TOKEN_STRING) {
        term->kind = TERM_NUMBER;
        if (!string_value(p, &term->value)) {
            return false;
        }
    } else if (t->type == TOKEN_DOLLAR) {
        term->kind = TERM_HERE;
    } else if (t->type == TOKEN_IDENT && !is_register(t) && !is_keyword(t)) {
        term->kind = TERM_SYMBOL;
        term->name = t->text;
        term->length = t->length;
    } else {
        return fail_at(p, "expected a number or a label");
    }
    p->pos++;
    return true;
}

/* term (('+' | '-') term)*, with an optional leading sign. */
static bool parse_expr(parser_t* p, expr_t* expr) {
    bool negative = false;

    expr->count = 0;
    if (at(p, TOKEN_PLUS) || at(p, TOKEN_MINUS)) {
        negative = at(p, TOKEN_MINUS);
        p->pos++;
    }
    for (;;) {
        if (expr->count == EXPR_MAX_TERMS) {
            return fail_at(p, "expression is too long");
        }
        term_t* term = &expr->terms[expr->count++];
        term->negative = negative;
        if (!parse_term(p, term)) {
            return false;
        }
        if (!at(p, TOKEN_PLUS) && !at(p, TOKEN_MINUS)) {
            return true;
        }
        negative = at(p, TOKEN_MINUS);
        p->pos++;
    }
}

/* ---- operands ----------------------------------------------------- */

/* [base], [base + disp], [base - disp], [address], [label + 4] ... */
static bool parse_memory(parser_t* p, operand_syntax_t* op) {
    expr_t* disp = &op->value;
    bool negative = false;

    op->type = OPERAND_MEM;
    op->base = NO_BASE;
    disp->count = 0;
    p->pos++;   /* '[' */
    for (;;) {
        const register_info_t* reg = at(p, TOKEN_IDENT) ? encoder_find_register(current(p)->text, current(p)->length) : NULL;
        if (reg) {
            if (op->base != NO_BASE || negative || reg->size != 32) {
                return fail_at(p, "an address can only add one 32-bit register");
            }
            op->base = reg->number;
            p->pos++;
        } else {
            if (disp->count == EXPR_MAX_TERMS) {
                return fail_at(p, "address is too long");
            }
            term_t* term = &disp->terms[disp->count++];
            term->negative = negative;
            if (!parse_term(p, term)) {
                return false;
            }
        }
        if (at(p, TOKEN_RBRACKET)) {
            p->pos++;
            return true;
        }
        if (!at(p, TOKEN_PLUS) && !at(p, TOKEN_MINUS)) {
            return fail_at(p, "expected +, - or ] in address, found");
        }
        negative = at(p, TOKEN_MINUS);
        p->pos++;
    }
}

/* 'byte' / 'dword' set the size; 'strict' and 'near' are accepted for
   NASM compatibility and ignored, because jumps are always near here. */
static int parse_size_keywords(parser_t* p) {
    int size = 0;

    for (;;) {
        if (token_is(current(p), "byte")) {
            size = 8;
        } else if (token_is(current(p), "dword")) {
            size = 32;
        } else if (!token_is(current(p), "strict") && !token_is(current(p), "near")) {
            return size;
        }
        p->pos++;
    }
}

static bool parse_operand(parser_t* p, operand_syntax_t* op) {
    op->size = parse_size_keywords(p);
    if (at(p, TOKEN_LBRACKET)) {
        return parse_memory(p, op);
    }
    if (is_register(current(p))) {
        const register_info_t* reg = encoder_find_register(current(p)->text, current(p)->length);
        if (op->size && op->size != reg->size) {
            return fail_at(p, "size keyword doesn't match register");
        }
        op->type = OPERAND_REG;
        op->size = reg->size;
        op->reg = reg->number;
        p->pos++;
        return true;
    }
    op->type = OPERAND_IMM;
    return parse_expr(p, &op->value);
}

static bool parse_instruction(parser_t* p, statement_t* st) {
    st->kind = STATEMENT_INSTRUCTION;
    st->mnemonic = encoder_find_mnemonic(current(p)->text, current(p)->length);
    st->operand_count = 0;
    p->pos++;
    if (at(p, TOKEN_END)) {
        return true;
    }
    for (;;) {
        if (st->operand_count == 2) {
            return fail_at(p, "too many operands");
        }
        if (!parse_operand(p, &st->operands[st->operand_count++])) {
            return false;
        }
        if (at(p, TOKEN_END)) {
            return true;
        }
        if (!at(p, TOKEN_COMMA)) {
            return fail_at(p, "expected , between operands, found");
        }
        p->pos++;
    }
}

/* ---- directives --------------------------------------------------- */

/* db/dw/dd item, item, ...   Items are numbers, labels or (db only) strings. */
static bool parse_data(parser_t* p, statement_t* st) {
    st->kind = STATEMENT_DATA;
    st->data_size = token_is(current(p), "db") ? 1 : token_is(current(p), "dw") ? 2 : 4;
    st->item_count = 0;
    p->pos++;
    for (;;) {
        if (st->item_count == DATA_MAX_ITEMS) {
            return fail_at(p, "too many items on one line");
        }
        data_item_t* item = &st->items[st->item_count++];
        item->is_string = at(p, TOKEN_STRING) && st->data_size == 1 &&
                          (p->tokens[p->pos + 1].type == TOKEN_COMMA || p->tokens[p->pos + 1].type == TOKEN_END);
        if (item->is_string) {
            item->string = current(p);
            p->pos++;
        } else if (!parse_expr(p, &item->value)) {
            return false;
        }
        if (at(p, TOKEN_END)) {
            return true;
        }
        if (!at(p, TOKEN_COMMA)) {
            return fail_at(p, "expected , between items, found");
        }
        p->pos++;
    }
}

/* Everything after the label: times, then an instruction or data. */
static bool parse_body(parser_t* p, statement_t* st) {
    if (token_is(current(p), "times")) {
        p->pos++;
        st->has_times = true;
        if (!parse_expr(p, &st->times)) {
            return false;
        }
    }
    if (at(p, TOKEN_END)) {
        return st->has_times ? fail_at(p, "times needs something to repeat, found") : true;
    }
    if (is_data_directive(current(p))) {
        return parse_data(p, st);
    }
    if (is_mnemonic(current(p))) {
        return parse_instruction(p, st);
    }
    return fail_at(p, "unknown instruction");
}

/* 'name:' or 'name' followed by a keyword is a label; 'name equ x' is equ. */
static bool parse_label(parser_t* p, statement_t* st) {
    const token_t* first = current(p);
    const token_t* second = &p->tokens[p->pos + 1];

    if (first->type != TOKEN_IDENT || is_keyword(first) || is_register(first)) {
        return true;
    }
    if (second->type == TOKEN_COLON) {
        st->label = first;
        p->pos += 2;
    } else if (is_keyword(second)) {
        st->label = first;
        p->pos++;
    }
    return true;
}

bool parser_parse(const token_t* tokens, statement_t* st, asm_error_t* error) {
    parser_t p = { tokens, 0, error };

    st->label = NULL;
    st->kind = STATEMENT_EMPTY;
    st->has_times = false;
    parse_label(&p, st);
    if (st->label && token_is(current(&p), "equ")) {
        p.pos++;
        st->kind = STATEMENT_EQU;
        if (!parse_expr(&p, &st->equ_value)) {
            return false;
        }
        return at(&p, TOKEN_END) || fail_at(&p, "unexpected text after equ value");
    }
    if (!st->label && at(&p, TOKEN_IDENT) && !is_keyword(current(&p)) && !is_register(current(&p))) {
        return fail_at(&p, "unknown instruction");
    }
    return parse_body(&p, st);
}
