/*
 * string.c - string and memory helpers (see string.h).
 *
 * memcpy and memset32 use the x86 string instructions (rep movs / rep stos).
 * They move 4 bytes per step, which matters when copying a 3 MB frame to
 * the screen.
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

char* strcpy(char* dest, const char* src) {
    char* d = dest;

    while ((*d++ = *src++)) {
    }
    return dest;
}

char* strcat(char* dest, const char* src) {
    strcpy(dest + strlen(dest), src);
    return dest;
}

char* utoa(uint32_t value, char* out, int base) {
    const char* digits = "0123456789ABCDEF";
    char reversed[32];
    int count = 0;

    do {
        reversed[count++] = digits[value % base];
        value /= base;
    } while (value);

    for (int i = 0; i < count; i++) {
        out[i] = reversed[count - 1 - i];
    }
    out[count] = '\0';
    return out;
}

void* memset(void* ptr, int value, size_t count) {
    uint8_t* p = ptr;

    while (count--) {
        *p++ = (uint8_t)value;
    }
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t count) {
    void* d = dest;
    const void* s = src;
    size_t words = count / 4;
    size_t bytes = count % 4;

    asm volatile("rep movsl" : "+D"(d), "+S"(s), "+c"(words) :: "memory");
    asm volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(bytes) :: "memory");
    return dest;
}

void* memset32(uint32_t* ptr, uint32_t value, size_t count) {
    void* d = ptr;

    asm volatile("rep stosl" : "+D"(d), "+c"(count) : "a"(value) : "memory");
    return ptr;
}

int memcmp(const void* a, const void* b, size_t count) {
    const uint8_t* p = a;
    const uint8_t* q = b;

    for (size_t i = 0; i < count; i++) {
        if (p[i] != q[i]) {
            return p[i] - q[i];
        }
    }
    return 0;
}
