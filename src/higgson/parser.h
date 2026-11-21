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
    int count;
} hg_name_set_t;

typedef struct {
    hg_lexer_t* lexer;
    hg_emitter_t* emitter;
    hg_name_set_t symbols;
    hg_name_set_t labels_declared;
    hg_name_set_t labels_gotoed;
    int labels_gotoed_line[HG_MAX_NAMES];   /* first GOTO to each, for errors */
    hg_token_t curr_token;
    hg_token_t peek_token;
} hg_parser_t;

void hg_parser_init(hg_parser_t* parser, hg_lexer_t* lexer, hg_emitter_t* emitter);
void hg_parser_program(hg_parser_t* parser);

#endif
