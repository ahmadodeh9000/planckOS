#include "strings.h"


int32_t strcmp(const char* str1, const char* str2) {

    unsigned char* s1 = (unsigned char*) str1;
    unsigned char* s2 = (unsigned char*) str2;

    while(*s1 && *s1 == *s2) {
        ++s1;
        ++s2;
    }

    return *s1 - *s2;
}

int strncmp( const char * s1, const char * s2, size_t n )
{
    while ( n && *s1 && ( *s1 == *s2 ) )
    {
        ++s1;
        ++s2;
        --n;
    }
    if ( n == 0 )
    {
        return 0;
    }
    else
    {
        return ( *(unsigned char *)s1 - *(unsigned char *)s2 );
    }
}

uint32_t strlen(const char* str) {
    uint32_t len = 0;

    for (uint32_t i = 0; str[i] != '\0'; ++i) {
        len ++;
    }

    return len;
}

void strcpy(char* src, char* dest) {
    for (uint32_t index = 0; src[index] != '\0';++index) {
        dest[index] = src[index];
    }
}