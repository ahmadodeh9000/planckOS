/*
 * llm.h - run a small language model (llama2.c format) inside planckOS.
 *
 * Load a model and its tokenizer from the disk, give it a prompt, then
 * call llm_next() repeatedly: each call runs the model once and returns
 * at most one piece of text. Doing one token per call lets the GUI stay
 * responsive while the model writes (see docs/LLM.md).
 *
 *   llm_t* llm;
 *   if (llm_load("S260K.BIN", "TOK512.BIN", &llm) == LLM_OK) {
 *       llm_start(llm, "Once upon a time");
 *       const char* piece;
 *       llm_step_t step;
 *       while ((step = llm_next(llm, &piece)) <= LLM_STEP_TOKEN) {
 *           printf("%s", piece);
 *       }
 *       llm_free(llm);
 *   }
 *
 * Uses floating point: never call it from an IRQ handler.
 */
#ifndef llm_h__
#define llm_h__

#include "lib/common.h"

typedef struct llm llm_t;

typedef enum {
    LLM_OK,
    LLM_NO_FILE,        /* a file isn't on the disk (or there is no disk) */
    LLM_BAD_MODEL,      /* the model file is corrupted or truncated */
    LLM_BAD_TOKENIZER,  /* the tokenizer file doesn't match the model */
    LLM_NO_MEMORY,
} llm_status;

typedef enum {
    LLM_STEP_PROMPT,    /* read one prompt token; 'piece' is "" */
    LLM_STEP_TOKEN,     /* wrote one token; 'piece' is its text */
    LLM_STEP_END,       /* the model ended the text, or nothing is queued */
    LLM_STEP_FULL,      /* the context is full (seq_len tokens) */
} llm_step_t;

llm_status llm_load(const char* model_file, const char* tokenizer_file, llm_t** out);
const char* llm_status_message(llm_status status);
void llm_free(llm_t* llm);

/* 0 = always the likeliest token (repeatable); 0.5-1 = more varied. */
void llm_set_temperature(llm_t* llm, float temperature);

/* Forget everything and queue BOS + 'prompt'. False if it doesn't fit. */
bool llm_start(llm_t* llm, const char* prompt);

/* Queue more text after what's there (no BOS, no leading space), e.g. the
   user's next message. False if it doesn't fit in the context. */
bool llm_feed(llm_t* llm, const char* text);

/* Run the model once. '*piece' stays valid until the next call. */
llm_step_t llm_next(llm_t* llm, const char** piece);

/* How many tokens 'text' takes (with BOS and the leading space if
   'first'), or -1. Lets callers plan what fits in the context. */
int llm_count_tokens(llm_t* llm, const char* text, bool first);

/* Tokens in use (read, queued or waiting to be read), and the maximum. */
int llm_tokens_used(llm_t* llm);
int llm_context_length(llm_t* llm);

#endif
