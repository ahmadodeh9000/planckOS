/*
 * lex.h - the Higgson lexer: turns the source text into tokens.
 */
#ifndef _lex__h
#define _lex__h

#include "higgson/platform.h"

#define HG_NAME_MAX     24      /* longest name: with its label prefix it
                                   must fit the assembler's 31 characters */
#define HG_STRING_MAX   200     /* longest string, to fit one assembly line */

// hg tokens


typedef enum {
	TOK_EOF,
	TOK_NUMBER,
	TOK_IDENT,
	TOK_STRING,

	// KEYWORDS
	TOK_LET,
	TOK_IF,
	TOK_ELSE,
	TOK_WHILE,
	TOK_PRINT,
	TOK_FUNC,
	TOK_RETURN,

	// OPS
	TOK_EQ,
	TOK_PLUS,
	TOK_MINUS,
	TOK_SLASH,
	TOK_STAR, // *
	TOK_PERCENT,
	TOK_EQEQ, // == 
	TOK_LT,
	TOK_NOTE,
	TOK_LTE,
	TOK_GT,
	TOK_GTE,
	TOK_AND, // &&
	TOK_OR,  // ||
	TOK_NOT, // !

	// PUNCTUATION
	TOK_LPAREN,
	TOK_RPAREN,
	TOK_LBRACE,
	TOK_RBRACE,
	TOK_COMMA,
	TOK_SEMICOLON


} hg_token_type_t;


typedef struct {
	const char* text;
	hg_token_type_t type;
	int line;               /* the source line it's on, from 1 */
} hg_token_t;


// hg lexer

typedef struct {
	const char* src;
	int length;             /* of src, counted once instead of per character */
	char curr_char;
	int curr_pos;
	int line;               /* the line curr_char is on */
} hg_lexer_t;

// lexer functions
void hg_lexer_init(hg_lexer_t* lexer, const char* src, int length);
void hg_lexer_next_char(hg_lexer_t* lexer);
char hg_lexer_peek(hg_lexer_t* lexer);

// tokens functions
hg_token_t hg_get_token(hg_lexer_t* lexer);



#endif
