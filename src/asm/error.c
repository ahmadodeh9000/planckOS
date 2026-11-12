/*
 * error.c - builds the assembler's error messages without overflowing.
 */
#include "asm/error.h"
#include "lib/string.h"

/* Append up to 'length' characters of 'text', always leaving room for '\0'. */
static void append(char* buffer, const char* text, uint32_t length) {
    uint32_t used = strlen(buffer);

    for (uint32_t i = 0; i < length && text[i] && used < ASM_ERROR_MAX - 1; i++) {
        buffer[used++] = text[i];
    }
    buffer[used] = '\0';
}

bool asm_fail(asm_error_t* error, const char* message, const char* detail, uint32_t detail_length) {
    error->message[0] = '\0';
    append(error->message, message, ASM_ERROR_MAX);
    if (detail) {
        append(error->message, ": ", 2);
        append(error->message, detail, detail_length);
    }
    return false;
}
