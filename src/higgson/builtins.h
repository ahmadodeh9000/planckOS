/*
 * builtins.h - Higgson's built-in functions and values: GUI apps, the
 * clock and the colors (docs/HIGGSON.md, "GUI apps").
 *
 * The parser asks here first whenever it meets a name, so these names
 * work like keywords: a program can't use them for its own variables or
 * funcs.
 */
#ifndef higgson_builtins_h__
#define higgson_builtins_h__

#include "higgson/parser.h"

/* The int 0x80 calls and window events the generated code may use, as
   "NAME equ value" lines for the header (src/arch/syscall.h). */
extern const char* const hg_builtin_constants[];
extern const uint32_t hg_builtin_constant_count;

/* True if 'name' is a built-in function or value. */
bool hg_is_builtin(const char* name);

/* If the current token starts a built-in statement (window(...);,
   rect(...);, on_click(...); ...), parse and emit it and return true. */
bool hg_builtin_statement(hg_parser_t* parser);

/* If the current token is a built-in value (mouse_x, hour(), RED, ...),
   emit code that puts it in eax and return true. */
bool hg_builtin_value(hg_parser_t* parser);

/* After the whole program: the checks that need all of it, and the code
   the built-ins need once (event entries, the number helper). */
void hg_builtins_finish(hg_parser_t* parser);

#endif
