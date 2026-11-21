/*
 * higgson.h - the Higgson compiler: Higgson source in, planckOS assembly out.
 *
 * The assembly is text for planckOS's own assembler (src/asm), which the
 * higgson command then turns into a program. docs/HIGGSON.md describes the
 * language and how the compiler works.
 */
#ifndef higgson_h__
#define higgson_h__

#include "higgson/platform.h"
#include "higgson/arena.h"

#define HG_ERROR_MAX    96      /* longest error message, with its '\0' */

typedef struct {
    bool ok;
    const char* text;           /* ok: the assembly, '\0'-terminated */
    uint32_t length;            /* ok: its length */
    int line;                   /* !ok: line of the error, 0 if it has none */
    char message[HG_ERROR_MAX]; /* !ok: what went wrong */
    hg_arena_t arena;           /* all the compiler's memory, 'text' included */
} hg_result_t;

/* Compile 'length' bytes of 'source' (no '\0' needed). Always call
   hg_result_free afterwards, whether it worked or not. Not reentrant: the
   compiler keeps its error state in static memory. */
hg_result_t hg_compile(const char* source, uint32_t length);

/* Frees all the compiler's memory in one go; 'text' is gone afterwards,
   'message' and 'line' stay readable. */
void hg_result_free(hg_result_t* result);

#endif
