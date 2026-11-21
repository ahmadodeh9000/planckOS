/*
 * hg.h - what the parts of the Higgson compiler share: memory from the
 * current compilation's arena, and stopping at an error.
 */
#ifndef higgson_hg_h__
#define higgson_hg_h__

#include "higgson/platform.h"

/* 'size' bytes from the arena. Never returns NULL: a full arena is an
   error ("program too big") like any other. */
void* hg_alloc(uint32_t size);

/* Stop compiling with a printf-style message. 'line' is the source line
   it's about (from 1), or 0 if none. Never returns: control goes back to
   hg_compile, which reports the error. */
void hg_error(int line, const char* fmt, ...) __attribute__((noreturn));

#endif
