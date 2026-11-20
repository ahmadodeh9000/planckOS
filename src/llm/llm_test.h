/*
 * llm_test.h - the 'llm-test' shell command: checks the port against llama2.c.
 */
#ifndef llm_test_h__
#define llm_test_h__

#include "lib/console.h"

/* Run the test model at temperature 0 and compare its text with what
   llama2.c's run.c wrote on a Mac. 'model' and 'tokenizer' may be NULL
   for the defaults (S260K.BIN, TOK512.BIN). Prints the result to 'con';
   returns true if every token matched. */
bool llm_test(console_t* con, const char* model, const char* tokenizer);

#endif
