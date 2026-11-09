/*
 * string.h - the few C string and memory routines the kernel needs.
 *
 * There is no C library, so these are our own. memset and memcpy also have
 * to exist because GCC may emit calls to them on its own, e.g. for large
 * struct copies, even with -ffreestanding.
 */
#ifndef string_h__
#define string_h__

#include "lib/common.h"

int32_t strcmp(const char* str1, const char* str2);
int strncmp(const char* s1, const char* s2, size_t n);
uint32_t strlen(const char* str);

void* memset(void* ptr, int value, size_t count);
void* memcpy(void* dest, const void* src, size_t count);
int memcmp(const void* a, const void* b, size_t count);

#endif
