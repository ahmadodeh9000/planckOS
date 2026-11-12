/*
 * lexer.h - splits one line of assembly source into tokens.
 *
 *   loop: add eax, [ebx+4]   ; comment
 *   -> IDENT(loop) COLON IDENT(add) IDENT(eax) COMMA LBRACKET IDENT(ebx)
 *      PLUS NUMBER(4) RBRACKET END
 *
 * Tokens point into the source line; nothing is copied.
 */
#ifndef asm_lexer_h__
#define asm_lexer_h__

#include "lib/common.h"
#include "asm/error.h"

#define LEXER_MAX_TOKENS 64

typedef enum {
    TOKEN_END,          /* end of the line (or start of a ; comment) */
    TOKEN_IDENT,        /* mov, eax, loop, _start, .data ... */
    TOKEN_NUMBER,       /* 42, 0x2A */
    TOKEN_STRING,       /* "text" or 'text' (text excludes the quotes) */
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_DOLLAR,       /* $: the address of the current line */
} token_type;

typedef struct {
    token_type type;
    const char* text;   /* first character (inside the quotes for strings) */
    uint32_t length;
    uint32_t value;     /* for TOKEN_NUMBER */
} token_t;

/* Tokenize 'line' (which may end at '\n' or '\0', at most 'length' bytes).
   The last token is always TOKEN_END. Returns false on a lexical error. */
bool lexer_tokenize(const char* line, uint32_t length, token_t* tokens, asm_error_t* error);

/* Case-insensitive compare of a token with a lowercase word like "mov". */
bool token_is(const token_t* token, const char* word);

#endif
