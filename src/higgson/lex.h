/*
 * lex.h - the Higgson lexer: turns the source text into tokens.
 */
#ifndef _lex__h
#define _lex__h

#include "higgson/platform.h"

// hg tokens


typedef enum {
	TOK_EOF,
	TOK_NEWLINE,
	TOK_NUMBER,
	TOK_IDENT,
	TOK_STRING,

	// KEWWORDS
	TOK_LET,
	TOK_GOTO,
	TOK_LABEL,
	TOK_IF,
	TOK_ELSE,
	TOK_THEN,
	TOK_ENDIF,
	TOK_WHILE,
	TOK_ENDWHILE,
	TOK_PRINT,
	TOK_INPUT,
	TOK_REPEAT,

	// OPS
	TOK_EQ,
	TOK_PLUS,
	TOK_MINUS,
	TOK_SLASH,
	TOK_STAR, // *
	TOK_EQEQ, // == 
	TOK_LT,
	TOK_NOTE,
	TOK_LTE,
	TOK_GT,
	TOK_GTE


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
