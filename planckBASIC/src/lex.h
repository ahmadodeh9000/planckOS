#ifndef _lex__h
#define _lex__h

// pcl tokens


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


} pcl_token_type_t;


typedef struct {
	const char* text;
	pcl_token_type_t type;
} pcl_token_t;


// pcl lexer

typedef struct {
	const char* src;
	char curr_char;
	int curr_pos;
	 

} pcl_lexer_t;

// lexer functions
void pcl_lexer_init(pcl_lexer_t* pcl, const char* src);
void pcl_lexer_next_char(pcl_lexer_t* lexer);
char pcl_lexer_peek(pcl_lexer_t* lexer);

// error functions
void pcl_abort(const char* err_msg);

// tokens functions
pcl_token_t pcl_get_token(pcl_lexer_t* lexer);



#endif
