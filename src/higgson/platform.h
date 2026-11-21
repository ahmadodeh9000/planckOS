/*
 * platform.h - all the Higgson compiler needs from the system it runs on.
 *
 * The compiler core (everything else in src/higgson/) uses no C library
 * and no kernel code, only what's declared here: memory, output text and
 * errors. So the same source files build twice:
 *
 *   src/higgson/platform_kernel.c        inside planckOS (kmalloc, ksnprintf,
 *                                        setjmp/longjmp from arch/jump.s)
 *   tools/higgson-host/platform_host.c   on the Mac or Linux (the C library)
 *
 * and both builds compile a program to the same assembly, byte for byte.
 */
#ifndef higgson_platform_h__
#define higgson_platform_h__

/* These come with the compiler, not with a C library. */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---- memory ---- */

/* One block for a whole compilation (the arena hands it out in pieces);
   NULL if there isn't that much memory. */
void* hg_platform_alloc(uint32_t size);
void hg_platform_free(void* block);

/* ---- output text ---- */

/* Format like snprintf, with %s %c %d %ld %u and %%: writes at most
   capacity - 1 characters and a '\0', and returns the length the whole
   text would have had. */
int hg_platform_vformat(char* out, uint32_t capacity, const char* fmt, va_list args);

/* ---- errors ---- */

typedef void (*hg_body_t)(void* arg);

/* Run body(arg). If hg_platform_fail is called anywhere inside it, however
   deep, stop right there and return false; otherwise return true. */
bool hg_platform_try(hg_body_t body, void* arg);

/* Only valid inside hg_platform_try's body. Never returns. */
void hg_platform_fail(void) __attribute__((noreturn));

#endif
