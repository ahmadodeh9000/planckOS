/*
 * error.h - the error message shared by the assembler's parts.
 *
 * The lexer, parser, encoder and assembler all report problems the same
 * way: fill in an asm_error_t and return false (or -1). The assembler adds
 * the line number and stops at the first error.
 */
#ifndef asm_error_h__
#define asm_error_h__

#include "lib/common.h"

#define ASM_ERROR_MAX 80

typedef struct {
    char message[ASM_ERROR_MAX];
} asm_error_t;

/* Set the message to "message" or "message: detail" (detail may be NULL;
   detail_length limits how much of it is copied). Always returns false, so
   callers can write 'return asm_fail(...)'. */
bool asm_fail(asm_error_t* error, const char* message, const char* detail, uint32_t detail_length);

#endif
