/*
 * lexer.c - turns a line of characters into tokens.
 *
 * It only knows the shape of words, numbers and punctuation; whether "eax"
 * is a register or "loop" a label is decided later by the parser. Every
 * read is bounded by the line length, so a malformed line can only produce
 * an error, never a read past its end.
 */
#include "asm/lexer.h"

#define HEX_BASE        16
#define DECIMAL_BASE    10
#define MAX_U32         0xFFFFFFFFu

typedef struct {
    const char* line;
    uint32_t length;
    uint32_t pos;
} cursor_t;

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.';
}

static bool is_ident_char(char c) {
    return is_ident_start(c) || is_digit(c);
}

static char peek(const cursor_t* c, uint32_t ahead) {
    uint32_t at = c->pos + ahead;
    return at < c->length ? c->line[at] : '\0';
}

static int digit_value(char c) {
    if (is_digit(c)) {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/* Decimal, or hex with a 0x prefix. Checks for overflow past 32 bits. */
static bool lex_number(cursor_t* c, token_t* token, asm_error_t* error) {
    uint32_t base = DECIMAL_BASE;
    uint64_t value = 0;

    if (peek(c, 0) == '0' && (peek(c, 1) == 'x' || peek(c, 1) == 'X')) {
        base = HEX_BASE;
        c->pos += 2;
    }
    uint32_t start = c->pos;
    while (is_ident_char(peek(c, 0))) {
        int digit = digit_value(peek(c, 0));
        if (digit < 0 || (uint32_t)digit >= base) {
            uint32_t shown = &c->line[c->pos] - token->text + 1;   /* up to the bad digit */
            return asm_fail(error, "bad digit in number", token->text, shown);
        }
        value = value * base + digit;
        if (value > MAX_U32) {
            return asm_fail(error, "number too big for 32 bits", NULL, 0);
        }
        c->pos++;
    }
    if (c->pos == start) {
        return asm_fail(error, "number has no digits", NULL, 0);
    }
    token->type = TOKEN_NUMBER;
    token->value = (uint32_t)value;
    return true;
}

static bool lex_string(cursor_t* c, token_t* token, asm_error_t* error) {
    char quote = peek(c, 0);
    uint32_t start = ++c->pos;

    while (c->pos < c->length && c->line[c->pos] != quote && c->line[c->pos] != '\n') {
        c->pos++;
    }
    if (peek(c, 0) != quote) {
        return asm_fail(error, "string has no closing quote", NULL, 0);
    }
    token->type = TOKEN_STRING;
    token->text = &c->line[start];
    token->length = c->pos - start;
    c->pos++;
    return true;
}

static bool lex_punctuation(cursor_t* c, token_t* token, asm_error_t* error) {
    static const struct { char c; token_type type; } marks[] = {
        { ',', TOKEN_COMMA },    { ':', TOKEN_COLON }, { '[', TOKEN_LBRACKET },
        { ']', TOKEN_RBRACKET }, { '+', TOKEN_PLUS },  { '-', TOKEN_MINUS },
        { '$', TOKEN_DOLLAR },
    };
    char ch = peek(c, 0);

    for (uint32_t i = 0; i < sizeof(marks) / sizeof(marks[0]); i++) {
        if (marks[i].c == ch) {
            token->type = marks[i].type;
            c->pos++;
            return true;
        }
    }
    return asm_fail(error, "unexpected character", &c->line[c->pos], 1);
}

/* Read one token starting at a non-space character. */
static bool lex_token(cursor_t* c, token_t* token, asm_error_t* error) {
    char ch = peek(c, 0);

    token->text = &c->line[c->pos];
    token->value = 0;
    if (is_digit(ch)) {
        if (!lex_number(c, token, error)) {
            return false;
        }
    } else if (is_ident_start(ch)) {
        while (is_ident_char(peek(c, 0))) {
            c->pos++;
        }
        token->type = TOKEN_IDENT;
    } else if (ch == '"' || ch == '\'') {
        return lex_string(c, token, error);
    } else if (!lex_punctuation(c, token, error)) {
        return false;
    }
    token->length = &c->line[c->pos] - token->text;
    return true;
}

bool lexer_tokenize(const char* line, uint32_t length, token_t* tokens, asm_error_t* error) {
    cursor_t c = { line, length, 0 };
    int count = 0;

    for (;;) {
        while (peek(&c, 0) == ' ' || peek(&c, 0) == '\t' || peek(&c, 0) == '\r') {
            c.pos++;
        }
        char ch = peek(&c, 0);
        if (ch == '\0' || ch == '\n' || ch == ';') {
            break;
        }
        if (count == LEXER_MAX_TOKENS - 1) {
            return asm_fail(error, "line has too many tokens", NULL, 0);
        }
        if (!lex_token(&c, &tokens[count], error)) {
            return false;
        }
        count++;
    }
    tokens[count].type = TOKEN_END;
    tokens[count].text = &line[c.pos];
    tokens[count].length = 0;
    return true;
}

bool token_is(const token_t* token, const char* word) {
    uint32_t i = 0;

    if (token->type != TOKEN_IDENT) {
        return false;
    }
    for (; i < token->length; i++) {
        char ch = token->text[i];
        if (ch >= 'A' && ch <= 'Z') {
            ch = ch - 'A' + 'a';
        }
        if (word[i] == '\0' || ch != word[i]) {
            return false;
        }
    }
    return word[i] == '\0';
}
