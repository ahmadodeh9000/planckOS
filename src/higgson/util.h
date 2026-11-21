/*
 * util.h - the small string and character helpers the compiler uses.
 *
 * Its own, instead of the C library's strlen or isdigit, so the compiler
 * core needs nothing but platform.h and builds the same in the kernel and
 * on a normal computer.
 */
#ifndef __util_h
#define __util_h

#include "higgson/platform.h"

int hg_util_strlen(const char* src);
int hg_util_strcmp(const char* a, const char* b);
void hg_util_memcpy(void* dest, const void* src, uint32_t count);

/* Like ctype's isdigit, isalpha and isalnum, for ASCII only. */
bool hg_util_is_digit(char c);
bool hg_util_is_alpha(char c);
bool hg_util_is_alnum(char c);

#endif
