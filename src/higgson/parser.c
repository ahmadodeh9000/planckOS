/*
 * parser.c - the Higgson parser: reads tokens and writes assembly.
 *
 * One function per grammar rule (recursive descent). Each rule emits the
 * code for what it parsed as it goes; an expression always leaves its
 * value in eax, using the stack for the left side of an operator.
 *
 * The grammar, from the top (docs/HIGGSON.md has it in words):
 *
 *   program    = { func | statement }
 *   func       = "func" name "(" ")" block
 *   block      = "{" { statement } "}"
 *   statement  = "let" name "=" expr ";"  |  name "=" expr ";"
 *              | name "(" ")" ";"  |  "print" "(" string | expr ")" ";"
 *              | "if" "(" expr ")" block [ "else" ( if | block ) ]
 *              | "while" "(" expr ")" block  |  "return" ";"
 *   expr       = and { "||" and }          and        = equality { "&&" equality }
 *   equality   = comparison { ("==" | "!=") comparison }
 *   comparison = expression { ("<" | "<=" | ">" | ">=") expression }
 *   expression = term { ("+" | "-") term }
 *   term       = unary { ("*" | "/" | "%") unary }
 *   unary      = ("-" | "+" | "!") unary | primary
 *   primary    = number | name | "(" expr ")"
 *
 * Built-in functions and values (window, rect, mouse_x, RED, ...) are in
 * builtins.c; the parser asks there first whenever it meets a name.
 */
#include "higgson/parser.h"
#include "higgson/builtins.h"
#include "higgson/hg.h"
#include "higgson/util.h"

#define INT_MAX_VALUE   2147483647UL
#define HEX_MAX_VALUE   4294967295UL    /* 0xFFFFFFFF: hex may use all 32 bits */
#define HG_MAX_DEPTH    16              /* nested blocks and parentheses: the
                                           kernel stack is small (8 KB) */

static int hg_set_find(const hg_name_set_t* set, const char* name) {
    for (int i = 0; i < set->count; ++i) {
        if (hg_util_strcmp(set->items[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

int hg_set_contains(const hg_name_set_t* set, const char* name) {
    return hg_set_find(set, name) >= 0;
}

void hg_set_add(hg_parser_t* parser, hg_name_set_t* set, const char* name) {
    if (hg_set_contains(set, name)) {
        return;
    }
    if (set->count >= HG_MAX_NAMES) {
        hg_error(parser->curr_token.line, "too many names (at most %d)", HG_MAX_NAMES);
    }
    set->lines[set->count] = parser->curr_token.line;
    set->items[set->count++] = name;
}

/* ---- tokens --------------------------------------------------------- */

int hg_check_token(hg_parser_t* parser, hg_token_type_t type) {
    return parser->curr_token.type == type;
}

void hg_next_token(hg_parser_t* parser) {
    parser->prev_line = parser->curr_token.line;
    parser->curr_token = parser->peek_token;
    parser->peek_token = hg_get_token(parser->lexer);
}

/* How an error message shows the current token: 'x' in quotes, or in
   words for the end of the file and strings. */
const char* hg_found(hg_parser_t* parser) {
    const char* text = parser->curr_token.text;

    switch (parser->curr_token.type) {
        case TOK_EOF:    return "the end of the file";
        case TOK_STRING: return "a string";
        default:         break;
    }
    int length = hg_util_strlen(text);
    char* quoted = hg_alloc(length + 3);
    quoted[0] = '\'';
    hg_util_memcpy(quoted + 1, text, length);
    quoted[length + 1] = '\'';
    quoted[length + 2] = '\0';
    return quoted;
}

/* Skip a token of the given type, or stop with "expected ..." */
void hg_match(hg_parser_t* parser, hg_token_type_t type, const char* text) {
    if (!hg_check_token(parser, type)) {
        hg_error(parser->curr_token.line, "expected '%s' but found %s", text, hg_found(parser));
    }
    hg_next_token(parser);
}

/* A missing ';' is reported on the line of the statement it should end,
   not on the next one, where the parser notices it. */
void hg_semicolon(hg_parser_t* parser) {
    if (!hg_check_token(parser, TOK_SEMICOLON)) {
        hg_error(parser->prev_line, "expected ';' after statement");
    }
    hg_next_token(parser);
}

/* Every block or parenthesis goes one level deeper into the parser's
   recursion, and so deeper into the kernel's small stack. */
static void hg_enter(hg_parser_t* parser) {
    if (++parser->depth > HG_MAX_DEPTH) {
        hg_error(parser->curr_token.line, "nested too deeply (at most %d levels)", HG_MAX_DEPTH);
    }
}

static void hg_leave(hg_parser_t* parser) {
    parser->depth--;
}

static void hg_statement(hg_parser_t* parser);
static void hg_comparison(hg_parser_t* parser);
static void hg_expression(hg_parser_t* parser);
static void hg_term(hg_parser_t* parser);
static void hg_unary(hg_parser_t* parser);
static void hg_primary(hg_parser_t* parser);

void hg_parser_init(hg_parser_t* parser, hg_lexer_t* lexer, hg_emitter_t* emitter) {
    *parser = (hg_parser_t){ 0 };
    parser->lexer = lexer;
    parser->emitter = emitter;

    hg_next_token(parser);
    hg_next_token(parser);
}

/* ---- program and funcs ---------------------------------------------- */

static void hg_function(hg_parser_t* parser);

void hg_parser_program(hg_parser_t* parser) {
    hg_emitter_t* e = parser->emitter;

    hg_header_line(e, "; generated by higgson");
    for (uint32_t i = 0; i < hg_builtin_constant_count; i++) {
        hg_header_line(e, "%s", hg_builtin_constants[i]);
        hg_emitter_symbol(e);
    }
    hg_header_line(e, "");
    hg_header_line(e, "_start:");
    hg_data_line(e, "_nl: db 10, 0");
    hg_emitter_symbol(e);       /* _start */
    hg_emitter_symbol(e);       /* _nl */

    while (!hg_check_token(parser, TOK_EOF)) {
        if (hg_check_token(parser, TOK_FUNC)) {
            hg_function(parser);
        }
        else {
            hg_statement(parser);
        }
    }

    /* The main code is done: back to whoever started the program. */
    hg_emit_line(e, "    xor eax, eax");
    hg_emit_line(e, "    ret");
    hg_emit_line(e, "");

    for (int i = 0; i < parser->funcs_called.count; ++i) {
        const char* name = parser->funcs_called.items[i];
        if (!hg_set_contains(&parser->funcs, name)) {
            hg_error(parser->funcs_called.lines[i], "unknown function '%s'", name);
        }
    }
    hg_builtins_finish(parser);
}

/* A name for a new variable or func: not taken by the other kind. */
static const char* hg_new_name(hg_parser_t* parser, const char* what) {
    const char* name = parser->curr_token.text;
    int line = parser->curr_token.line;

    if (!hg_check_token(parser, TOK_IDENT)) {
        hg_error(line, "expected a name for the %s but found %s", what, hg_found(parser));
    }
    if (hg_is_builtin(name)) {
        hg_error(line, "'%s' is a built-in name; choose another", name);
    }
    if (hg_set_contains(&parser->symbols, name)) {
        hg_error(line, "'%s' is already a variable", name);
    }
    if (hg_set_contains(&parser->funcs, name)) {
        hg_error(line, "'%s' is already a func", name);
    }
    hg_next_token(parser);
    return name;
}

static void hg_block(hg_parser_t* parser);

/* func name() { ... }: a function with a label, called with 'call'. Its
   code goes after the main code, wherever it is in the source. */
static void hg_function(hg_parser_t* parser) {
    hg_emitter_t* e = parser->emitter;

    hg_next_token(parser);
    const char* name = hg_new_name(parser, "func");
    hg_set_add(parser, &parser->funcs, name);
    hg_emitter_symbol(e);
    hg_match(parser, TOK_LPAREN, "(");
    if (!hg_check_token(parser, TOK_RPAREN)) {
        hg_error(parser->curr_token.line, "funcs don't take parameters (yet)");
    }
    hg_match(parser, TOK_RPAREN, ")");

    hg_emitter_section(e, HG_SECTION_FUNCS);
    parser->func = name;
    hg_emit_line(e, "_f_%s:", name);
    hg_block(parser);
    hg_emit_line(e, "    ret");
    hg_emit_line(e, "");
    parser->func = NULL;
    hg_emitter_section(e, HG_SECTION_MAIN);
}

/* ---- statements ----------------------------------------------------- */

static void hg_block(hg_parser_t* parser) {
    int open_line = parser->curr_token.line;

    hg_match(parser, TOK_LBRACE, "{");
    hg_enter(parser);
    while (!hg_check_token(parser, TOK_RBRACE)) {
        if (hg_check_token(parser, TOK_EOF)) {
            hg_error(open_line, "this '{' is never closed with '}'");
        }
        hg_statement(parser);
    }
    hg_leave(parser);
    hg_next_token(parser);
}

static void hg_emit_print_newline(hg_emitter_t* e) {
    hg_emit_line(e, "    mov eax, SYS_PRINT");
    hg_emit_line(e, "    mov ebx, _nl");
    hg_emit_line(e, "    int 0x80");
}

/* print("text"); or print(expression); and a newline after either. */
static void hg_print_statement(hg_parser_t* parser) {
    hg_emitter_t* e = parser->emitter;

    hg_next_token(parser);
    hg_match(parser, TOK_LPAREN, "(");
    if (hg_check_token(parser, TOK_STRING)) {
        int label = hg_new_label(e);
        hg_data_line(e, "S%d: db \"%s\", 10, 0", label, parser->curr_token.text);
        hg_emit_line(e, "    mov eax, SYS_PRINT");
        hg_emit_line(e, "    mov ebx, S%d", label);
        hg_emit_line(e, "    int 0x80");
        hg_next_token(parser);
    }
    else {
        hg_value(parser);
        hg_emit_line(e, "    mov ebx, eax");
        hg_emit_line(e, "    mov eax, SYS_PRINT_NUMBER");
        hg_emit_line(e, "    int 0x80");
        hg_emit_print_newline(e);
    }
    hg_match(parser, TOK_RPAREN, ")");
    hg_semicolon(parser);
}

/* "(" expr ")", then skip to 'label' if it was 0 (false). */
static void hg_condition(hg_parser_t* parser, int label) {
    hg_match(parser, TOK_LPAREN, "(");
    hg_value(parser);
    hg_match(parser, TOK_RPAREN, ")");
    hg_emit_line(parser->emitter, "    cmp eax, 0");
    hg_emit_line(parser->emitter, "    je L%d", label);
}

static void hg_if_statement(hg_parser_t* parser) {
    hg_emitter_t* e = parser->emitter;
    int otherwise = hg_new_label(e);

    hg_next_token(parser);
    hg_condition(parser, otherwise);
    hg_block(parser);

    if (!hg_check_token(parser, TOK_ELSE)) {
        hg_emit_line(e, "L%d:", otherwise);
        return;
    }
    int end = hg_new_label(e);
    hg_emit_line(e, "    jmp L%d", end);
    hg_emit_line(e, "L%d:", otherwise);
    hg_next_token(parser);
    if (hg_check_token(parser, TOK_IF)) {
        hg_enter(parser);           /* else if ... is one level deeper */
        hg_if_statement(parser);
        hg_leave(parser);
    }
    else {
        hg_block(parser);
    }
    hg_emit_line(e, "L%d:", end);
}

static void hg_while_statement(hg_parser_t* parser) {
    hg_emitter_t* e = parser->emitter;
    int top = hg_new_label(e);
    int end = hg_new_label(e);

    hg_next_token(parser);
    hg_emit_line(e, "L%d:", top);
    hg_condition(parser, end);
    hg_block(parser);
    hg_emit_line(e, "    jmp L%d", top);
    hg_emit_line(e, "L%d:", end);
}

/* let name = expression; declares the variable (every variable is a
   global: one dd in the data, whether declared in main code or a func). */
static void hg_let_statement(hg_parser_t* parser) {
    hg_next_token(parser);
    const char* name = hg_new_name(parser, "variable");

    hg_set_add(parser, &parser->symbols, name);
    hg_emitter_symbol(parser->emitter);
    hg_data_line(parser->emitter, "_v_%s: dd 0", name);

    hg_match(parser, TOK_EQ, "=");
    hg_value(parser);
    hg_emit_line(parser->emitter, "    mov [_v_%s], eax", name);
    hg_semicolon(parser);
}

/* return; leaves a func, or ends the main code. The stack is balanced
   between statements, so a plain ret is always safe here. */
static void hg_return_statement(hg_parser_t* parser) {
    hg_next_token(parser);
    if (!parser->func) {
        hg_emit_line(parser->emitter, "    xor eax, eax");
    }
    hg_emit_line(parser->emitter, "    ret");
    hg_semicolon(parser);
}

/* name(); calls a func, which may be defined further down. */
static void hg_call_statement(hg_parser_t* parser, const char* name) {
    if (hg_set_contains(&parser->symbols, name)) {
        hg_error(parser->curr_token.line, "'%s' is a variable, not a func", name);
    }
    hg_set_add(parser, &parser->funcs_called, name);
    hg_next_token(parser);
    hg_match(parser, TOK_LPAREN, "(");
    if (!hg_check_token(parser, TOK_RPAREN)) {
        hg_error(parser->curr_token.line, "funcs don't take arguments (yet)");
    }
    hg_match(parser, TOK_RPAREN, ")");
    hg_emit_line(parser->emitter, "    call _f_%s", name);
    hg_semicolon(parser);
}

/* name = expression; or name(); */
static void hg_name_statement(hg_parser_t* parser) {
    const char* name = parser->curr_token.text;

    if (hg_builtin_statement(parser)) {
        return;
    }
    if (parser->peek_token.type == TOK_LPAREN) {
        hg_call_statement(parser, name);
        return;
    }
    if (parser->peek_token.type != TOK_EQ) {
        hg_next_token(parser);
        hg_error(parser->curr_token.line, "expected '=' or '(' after '%s'", name);
    }
    if (!hg_set_contains(&parser->symbols, name)) {
        hg_error(parser->curr_token.line, "unknown variable '%s'", name);
    }
    hg_next_token(parser);
    hg_next_token(parser);
    hg_value(parser);
    hg_emit_line(parser->emitter, "    mov [_v_%s], eax", name);
    hg_semicolon(parser);
}

static void hg_statement(hg_parser_t* parser) {
    switch (parser->curr_token.type) {
        case TOK_PRINT:  hg_print_statement(parser); break;
        case TOK_IF:     hg_if_statement(parser); break;
        case TOK_WHILE:  hg_while_statement(parser); break;
        case TOK_LET:    hg_let_statement(parser); break;
        case TOK_RETURN: hg_return_statement(parser); break;
        case TOK_IDENT:  hg_name_statement(parser); break;
        case TOK_FUNC:
            hg_error(parser->curr_token.line, parser->func
                     ? "a func can't be inside another func"
                     : "a func can't be inside if or while");
        case TOK_ELSE:
            hg_error(parser->curr_token.line, "'else' without an 'if' before it");
        default:
            hg_error(parser->curr_token.line, "expected a statement but found %s",
                     hg_found(parser));
    }
}

/* ---- expressions ---------------------------------------------------- */

static void hg_emit_pop_left(hg_emitter_t* e) {
    hg_emit_line(e, "    mov ecx, eax");
    hg_emit_line(e, "    pop eax");
}

/* Turn eax into 1 if it isn't 0 (true), else 0, the way C does. */
static void hg_emit_truth(hg_emitter_t* e, const char* jump_if_true) {
    int done = hg_new_label(e);
    hg_emit_line(e, "    mov eax, 1");
    hg_emit_line(e, "    %s L%d", jump_if_true, done);
    hg_emit_line(e, "    mov eax, 0");
    hg_emit_line(e, "L%d:", done);
}

/* && and || share this: 'stop' is the jump that ends the chain early
   ("je" for &&: one false is enough; "jne" for ||: one true is enough),
   'early' is the chain's value then (0 or 1). The right side is never
   computed if the left one decides, as in C. */
static void hg_logic(hg_parser_t* parser, hg_token_type_t op, void (*side)(hg_parser_t*),
                     const char* stop, int early) {
    hg_emitter_t* e = parser->emitter;

    side(parser);
    if (!hg_check_token(parser, op)) {
        return;
    }
    int decided = hg_new_label(e);
    int done = hg_new_label(e);
    while (hg_check_token(parser, op)) {
        hg_next_token(parser);
        hg_emit_line(e, "    cmp eax, 0");
        hg_emit_line(e, "    %s L%d", stop, decided);
        side(parser);
    }
    hg_emit_line(e, "    cmp eax, 0");
    hg_emit_line(e, "    %s L%d", stop, decided);
    hg_emit_line(e, "    mov eax, %d", !early);
    hg_emit_line(e, "    jmp L%d", done);
    hg_emit_line(e, "L%d:", decided);
    hg_emit_line(e, "    mov eax, %d", early);
    hg_emit_line(e, "L%d:", done);
}

static void hg_equality(hg_parser_t* parser);

/* "&&" groups tighter than "||", as in C: a || b && c is a || (b && c). */
static void hg_and(hg_parser_t* parser) {
    hg_logic(parser, TOK_AND, hg_equality, "je", 0);
}

/* A whole expression, || being the loosest operator. */
void hg_value(hg_parser_t* parser) {
    hg_logic(parser, TOK_OR, hg_and, "jne", 1);
}

static const char* hg_jump_for(hg_token_type_t type) {
    switch (type) {
        case TOK_GT:   return "jg";
        case TOK_GTE:  return "jge";
        case TOK_LT:   return "jl";
        case TOK_LTE:  return "jle";
        case TOK_EQEQ: return "je";
        case TOK_NOTE: return "jne";
        default:       return "jmp";
    }
}

static int hg_is_comparison_operator(hg_parser_t* parser) {
    return hg_check_token(parser, TOK_GT) || hg_check_token(parser, TOK_GTE) ||
           hg_check_token(parser, TOK_LT) || hg_check_token(parser, TOK_LTE);
}

static int hg_is_equality_operator(hg_parser_t* parser) {
    return hg_check_token(parser, TOK_EQEQ) || hg_check_token(parser, TOK_NOTE);
}

/* One comparison: left in eax is compared with the right side, giving
   1 or 0. */
static void hg_compare(hg_parser_t* parser, void (*side)(hg_parser_t*)) {
    hg_emitter_t* e = parser->emitter;
    hg_token_type_t op = parser->curr_token.type;

    hg_next_token(parser);
    hg_emit_line(e, "    push eax");
    side(parser);
    hg_emit_pop_left(e);
    hg_emit_line(e, "    cmp eax, ecx");
    hg_emit_truth(e, hg_jump_for(op));
}

static void hg_equality(hg_parser_t* parser) {
    hg_comparison(parser);

    while (hg_is_equality_operator(parser)) {
        hg_compare(parser, hg_comparison);
    }
}

static void hg_comparison(hg_parser_t* parser) {
    hg_expression(parser);

    while (hg_is_comparison_operator(parser)) {
        hg_compare(parser, hg_expression);
    }
}

static void hg_expression(hg_parser_t* parser) {
    hg_emitter_t* e = parser->emitter;

    hg_term(parser);

    while (hg_check_token(parser, TOK_PLUS) || hg_check_token(parser, TOK_MINUS)) {
        hg_token_type_t op = parser->curr_token.type;
        hg_next_token(parser);

        hg_emit_line(e, "    push eax");
        hg_term(parser);
        hg_emit_pop_left(e);

        if (op == TOK_PLUS) {
            hg_emit_line(e, "    add eax, ecx");
        }
        else {
            hg_emit_line(e, "    sub eax, ecx");
        }
    }
}

static int hg_is_term_operator(hg_parser_t* parser) {
    return hg_check_token(parser, TOK_STAR) || hg_check_token(parser, TOK_SLASH) ||
           hg_check_token(parser, TOK_PERCENT);
}

static void hg_term(hg_parser_t* parser) {
    hg_emitter_t* e = parser->emitter;

    hg_unary(parser);

    while (hg_is_term_operator(parser)) {
        hg_token_type_t op = parser->curr_token.type;
        hg_next_token(parser);

        hg_emit_line(e, "    push eax");
        hg_unary(parser);
        hg_emit_pop_left(e);

        if (op == TOK_STAR) {
            hg_emit_line(e, "    mul ecx");
        }
        else {
            /* edx:eax / ecx: the quotient lands in eax, the rest in edx */
            hg_emit_line(e, "    cdq");
            hg_emit_line(e, "    idiv ecx");
            if (op == TOK_PERCENT) {
                hg_emit_line(e, "    mov eax, edx");
            }
        }
    }
}

static void hg_unary(hg_parser_t* parser) {
    hg_emitter_t* e = parser->emitter;
    hg_token_type_t op = parser->curr_token.type;

    if (op != TOK_PLUS && op != TOK_MINUS && op != TOK_NOT) {
        hg_primary(parser);
        return;
    }
    hg_next_token(parser);
    hg_enter(parser);
    hg_unary(parser);           /* so - - x and !!x work */
    hg_leave(parser);

    if (op == TOK_MINUS) {
        hg_emit_line(e, "    neg eax");
    }
    else if (op == TOK_NOT) {
        hg_emit_line(e, "    cmp eax, 0");
        hg_emit_truth(e, "je");     /* !x is 1 exactly when x is 0 */
    }
}

/* The value of a number token. Our own strtol: no C library here. */
static uint32_t hg_parse_number(hg_parser_t* parser, const char* text) {
    bool hex = text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    uint32_t base = hex ? 16 : 10;
    uint32_t limit = hex ? HEX_MAX_VALUE : INT_MAX_VALUE;
    uint32_t value = 0;

    for (const char* p = hex ? text + 2 : text; *p; p++) {
        uint32_t digit = hg_util_is_digit(*p) ? (uint32_t)(*p - '0')
                       : (uint32_t)((*p | 0x20) - 'a' + 10);
        if (value > (limit - digit) / base) {
            hg_error(parser->curr_token.line, "number too big: %s (the largest is %s)",
                     text, hex ? "0xFFFFFFFF" : "2147483647");
        }
        value = value * base + digit;
    }
    return value;
}

static void hg_variable(hg_parser_t* parser) {
    const char* name = parser->curr_token.text;

    if (hg_builtin_value(parser)) {
        return;
    }
    if (parser->peek_token.type == TOK_LPAREN) {
        hg_error(parser->curr_token.line, "func '%s' gives no value: call it on its own line", name);
    }
    if (!hg_set_contains(&parser->symbols, name)) {
        hg_error(parser->curr_token.line, "unknown variable '%s'", name);
    }
    hg_emit_line(parser->emitter, "    mov eax, [_v_%s]", name);
    hg_next_token(parser);
}

static void hg_primary(hg_parser_t* parser) {
    hg_emitter_t* e = parser->emitter;

    if (hg_check_token(parser, TOK_NUMBER)) {
        hg_emit_line(e, "    mov eax, %u", hg_parse_number(parser, parser->curr_token.text));
        hg_next_token(parser);
    }
    else if (hg_check_token(parser, TOK_IDENT)) {
        hg_variable(parser);
    }
    else if (hg_check_token(parser, TOK_LPAREN)) {
        hg_next_token(parser);
        hg_enter(parser);
        hg_value(parser);
        hg_leave(parser);
        hg_match(parser, TOK_RPAREN, ")");
    }
    else if (hg_check_token(parser, TOK_STRING)) {
        hg_error(parser->curr_token.line, "a string can't be used in a calculation");
    }
    else {
        hg_error(parser->curr_token.line, "expected a value but found %s", hg_found(parser));
    }
}
