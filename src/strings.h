#ifndef strings_h__
#define strings_h__

#include "common.h"

int32_t strcmp(const char* str1, const char *str2);
uint32_t strlen(const char* str);
void strcpy(char* src, char* dest);
int strncmp( const char * s1, const char * s2, size_t n );

#endif