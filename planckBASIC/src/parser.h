#ifndef _parse__h
#define _parse__h

#include "lex.h"
#include "emit.h"

#define PCL_MAX_NAMES 256

typedef struct {
    const char* items[PCL_MAX_NAMES];
    int count;
} pcl_name_set_t;

typedef struct {
    pcl_lexer_t* lexer;
    pcl_emitter_t* emitter;
    pcl_name_set_t symbols;
    pcl_name_set_t labels_declared;
    pcl_name_set_t labels_gotoed;
    pcl_token_t curr_token;
    pcl_token_t peek_token;
} pcl_parser_t;

void pcl_parser_init(pcl_parser_t* parser, pcl_lexer_t* lexer, pcl_emitter_t* emitter);
void pcl_parser_program(pcl_parser_t* parser);

void pcl_parser_program(pcl_parser_t* parser);

#endif