/*
 * emit.h - the Higgson emitter: collects the assembly the parser writes.
 *
 * Four buffers, joined at the end in this order: the header (constants),
 * the main code, the funcs, and the data (strings and variables). So the
 * parser can add a variable or a string the moment it sees one, and a func
 * body ends up after the main code wherever it was in the source.
 */
#ifndef _emit__h
#define _emit__h

#include "higgson/platform.h"

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} hg_buffer_t;

/* planckOS's assembler holds at most this many labels and constants. */
#define HG_ASM_MAX_SYMBOLS 256

typedef enum {
    HG_SECTION_MAIN,        /* code that runs when the program starts */
    HG_SECTION_FUNCS        /* func bodies, after the main code */
} hg_section_t;

typedef struct {
    hg_buffer_t header;
    hg_buffer_t code;
    hg_buffer_t funcs;
    hg_buffer_t data;
    hg_buffer_t* target;    /* where hg_emit writes: code or funcs */
    int label_count;
    int symbol_count;       /* labels and constants defined so far */
} hg_emitter_t;

void hg_emitter_init(hg_emitter_t* emitter);
void hg_emit(hg_emitter_t* emitter, const char* fmt, ...);
void hg_emit_line(hg_emitter_t* emitter, const char* fmt, ...);
void hg_header_line(hg_emitter_t* emitter, const char* fmt, ...);
void hg_data_line(hg_emitter_t* emitter, const char* fmt, ...);
int  hg_new_label(hg_emitter_t* emitter);

/* Where hg_emit and hg_emit_line write from now on. */
void hg_emitter_section(hg_emitter_t* emitter, hg_section_t section);

/* Counts a label or constant the parser defines by name (a variable, a
   func, an equ), so the program never has more than the assembler holds. */
void hg_emitter_symbol(hg_emitter_t* emitter);

/* The whole program: header, code, funcs and data in one
   '\0'-terminated text. */
const char* hg_emitter_text(hg_emitter_t* emitter, uint32_t* length);

#endif
