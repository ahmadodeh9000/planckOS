/*
 * emit.h - the Higgson emitter: collects the assembly the parser writes.
 *
 * Three buffers, joined at the end: the header (constants), the code, and
 * the data (strings and variables), so the parser can add a variable or a
 * string the moment it sees one, wherever it is in the code.
 */
#ifndef _emit__h
#define _emit__h

#include "higgson/platform.h"

typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} hg_buffer_t;

typedef struct {
    hg_buffer_t header;
    hg_buffer_t code;
    hg_buffer_t data;
    int label_count;
} hg_emitter_t;

void hg_emitter_init(hg_emitter_t* emitter);
void hg_emit(hg_emitter_t* emitter, const char* fmt, ...);
void hg_emit_line(hg_emitter_t* emitter, const char* fmt, ...);
void hg_header_line(hg_emitter_t* emitter, const char* fmt, ...);
void hg_data_line(hg_emitter_t* emitter, const char* fmt, ...);
int  hg_new_label(hg_emitter_t* emitter);

/* The whole program: header, code and data in one '\0'-terminated text. */
const char* hg_emitter_text(hg_emitter_t* emitter, uint32_t* length);

#endif
