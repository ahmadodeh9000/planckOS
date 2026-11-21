/*
 * parser.h - the Higgson parser, which also drives the emitter.
 */
#ifndef _parse__h
#define _parse__h

#include "higgson/lex.h"
#include "higgson/emit.h"

#define HG_MAX_NAMES 256

typedef struct {
    const char* items[HG_MAX_NAMES];
    int lines[HG_MAX_NAMES];        /* where each was first seen, for errors */
    int count;
} hg_name_set_t;

typedef struct {
    hg_lexer_t* lexer;
    hg_emitter_t* emitter;
    hg_name_set_t symbols;          /* variables, declared with let */
    hg_name_set_t funcs;            /* funcs defined so far */
    hg_name_set_t funcs_called;     /* funcs used, checked at the end: a
                                       func may be defined after its use */
    hg_token_t curr_token;
    hg_token_t peek_token;
    int prev_line;                  /* line of the token before curr_token */
    const char* func;               /* the func being parsed, NULL in main */
    int depth;                      /* blocks and parentheses we're inside */
} hg_parser_t;

void hg_parser_init(hg_parser_t* parser, hg_lexer_t* lexer, hg_emitter_t* emitter);
void hg_parser_program(hg_parser_t* parser);

#endif
