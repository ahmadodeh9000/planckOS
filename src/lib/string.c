/*
 * string.c - string and memory helpers (see string.h).
 */
#include "lib/string.h"

int32_t strcmp(const char* str1, const char* str2) {
    const unsigned char* s1 = (const unsigned char*)str1;
    const unsigned char* s2 = (const unsigned char*)str2;

    while (*s1 && *s1 == *s2) {
        ++s1;
        ++s2;
    }
    return *s1 - *s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && *s1 == *s2) {
        ++s1;
        ++s2;
        --n;
    }
    if (n == 0) {
        return 0;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

uint32_t strlen(const char* str) {
    uint32_t len = 0;

    while (str[len] != '\0') {
        len++;
    }
    return len;
}

void* memset(void* ptr, int value, size_t count) {
    uint8_t* p = ptr;

    while (count--) {
        *p++ = (uint8_t)value;
    }
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = dest;
    const uint8_t* s = src;

    while (count--) {
        *d++ = *s++;
    }
    return dest;
}
