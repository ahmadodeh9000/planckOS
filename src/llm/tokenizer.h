/*
 * tokenizer.h - turns text into token numbers and back (byte pair encoding).
 *
 * The vocabulary comes from llama2.c's tokenizer file (tok512.bin and the
 * like): one string and one merge score per token.
 */
#ifndef tokenizer_h__
#define tokenizer_h__

#include "lib/common.h"

#define TOKEN_BOS   1       /* "beginning of sequence": starts every text */

typedef struct {
    char** vocab;               /* vocab_size strings, '\0'-terminated */
    int* vocab_length;          /* strlen of each, to skip most comparisons */
    float* vocab_scores;        /* higher = merged earlier */
    char* strings;              /* one block that holds all the strings */
    int vocab_size;
    int max_token_length;
    char byte_pieces[512];      /* "\x00\0\x01\0...": one string per byte */
} tokenizer_t;

/* Parse a tokenizer file. 'data' can be freed afterwards. Returns false,
   with the reason on serial, if the file doesn't fit 'vocab_size'. */
bool tokenizer_init(tokenizer_t* t, const uint8_t* data, uint32_t size, int vocab_size);
void tokenizer_free(tokenizer_t* t);

/* Encode 'text' into at most 'max_tokens' tokens. 'bos' puts TOKEN_BOS
   first; 'dummy_prefix' adds the leading space that SentencePiece adds to
   the start of every text it was trained on. Returns the number of
   tokens, or -1 if they didn't fit or memory ran out. */
int tokenizer_encode(tokenizer_t* t, const char* text, bool bos, bool dummy_prefix,
                     int* tokens, int max_tokens);

/* The text of 'token'. 'prev_token' matters only for TOKEN_BOS, after
   which the leading space is dropped. */
const char* tokenizer_decode(tokenizer_t* t, int prev_token, int token);

#endif
