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
char* strcpy(char* dest, const char* src);
char* strcat(char* dest, const char* src);

/* Write 'value' in 'base' (2-16) into 'out' as a '\0'-terminated string.
   'out' needs room for 33 bytes in the worst case (base 2). */
char* utoa(uint32_t value, char* out, int base);

void* memset(void* ptr, int value, size_t count);
void* memcpy(void* dest, const void* src, size_t count);
int memcmp(const void* a, const void* b, size_t count);

/* Fill 'count' 32-bit words with 'value' (used to fill pixel rows). */
void* memset32(uint32_t* ptr, uint32_t value, size_t count);

#endif
