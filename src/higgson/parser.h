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

    /* GUI apps (builtins.c) */
    int window_line;                /* where window() is; 0 = not (yet) */
    int window_needed_line;         /* first thing in a func that needs one */
    const char* window_needed_by;   /* ... and its name */
    bool gui_data;                  /* _hg_win, mouse_x, ... are in the data */
    bool uses_number;               /* number() needs the _hg_itoa helper */
    hg_name_set_t click_handlers;   /* funcs given to on_click: each gets an */
    hg_name_set_t key_handlers;     /* entry that saves the event's values */
} hg_parser_t;

void hg_parser_init(hg_parser_t* parser, hg_lexer_t* lexer, hg_emitter_t* emitter);
void hg_parser_program(hg_parser_t* parser);

/* ---- shared with builtins.c ---- */

int  hg_set_contains(const hg_name_set_t* set, const char* name);
void hg_set_add(hg_parser_t* parser, hg_name_set_t* set, const char* name);
int  hg_check_token(hg_parser_t* parser, hg_token_type_t type);
void hg_next_token(hg_parser_t* parser);
void hg_match(hg_parser_t* parser, hg_token_type_t type, const char* text);
void hg_semicolon(hg_parser_t* parser);

/* The current token for an error message: 'x' in quotes, or in words. */
const char* hg_found(hg_parser_t* parser);

/* A whole expression; its value ends up in eax. */
void hg_value(hg_parser_t* parser);

#endif
