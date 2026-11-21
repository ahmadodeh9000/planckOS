/*
 * util.c - string and character helpers (see util.h).
 */
#include "higgson/util.h"


int hg_util_strlen(const char* src) {
    const char* p = src;

    while (*p) 
        p++;
    
    return (p - src);
}

int hg_util_strcmp(const char* a, const char* b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

void hg_util_memcpy(void* dest, const void* src, uint32_t count) {
    uint8_t* d = dest;
    const uint8_t* s = src;

    while (count--) {
        *d++ = *s++;
    }
}

bool hg_util_is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool hg_util_is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool hg_util_is_alnum(char c) {
    return hg_util_is_alpha(c) || hg_util_is_digit(c);
}
