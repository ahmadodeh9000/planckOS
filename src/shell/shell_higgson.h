/*
 * shell_higgson.h - the shell's `higgson` command: compile a Higgson
 * program (docs/HIGGSON.md).
 */
#ifndef shell_higgson_h__
#define shell_higgson_h__

#include "lib/console.h"

#define HIGGSON_MAX_SOURCE  (32 * 1024)     /* largest .HGS file it reads */

/* Compile 'source' (read from in_name) and write out_name: the assembly
   text if its name ends in .ASM, otherwise the finished program. Errors
   and the result are printed on 'con'. */
void shell_higgson(console_t* con, const char* in_name, const char* out_name,
                   const char* source, uint32_t size);

#endif
