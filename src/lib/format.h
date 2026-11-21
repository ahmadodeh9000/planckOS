/*
 * format.h - snprintf for the kernel: formats text into a buffer.
 *
 * console_printf prints as it goes; this is for when the text is needed as
 * a string (the Higgson compiler writes its assembly with it). It knows
 * %s %c %d %ld %u and %%, which is all the kernel needs.
 */
#ifndef format_h__
#define format_h__

#include "lib/common.h"
#include <stdarg.h>

/* Like C's snprintf: writes at most capacity - 1 characters and a '\0'
   (nothing at all if capacity is 0), and returns the length the whole
   text would have had, so a caller can tell it was cut and retry with a
   bigger buffer. */
int ksnprintf(char* out, uint32_t capacity, const char* fmt, ...);
int kvsnprintf(char* out, uint32_t capacity, const char* fmt, va_list args);

#endif
