/*
 * platform_host.c - the Higgson compiler's platform layer on a normal
 * computer (macOS, Linux), built by 'make higgson-host'.
 *
 * The same three things as src/higgson/platform_kernel.c, from the C
 * library: malloc, vsnprintf, setjmp/longjmp. Compiling a program here and
 * inside planckOS gives the same assembly, byte for byte.
 */
#include "higgson/platform.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

static jmp_buf failed;      /* where hg_platform_fail jumps to */

void* hg_platform_alloc(uint32_t size) {
    return malloc(size);
}

void hg_platform_free(void* block) {
    free(block);
}

int hg_platform_vformat(char* out, uint32_t capacity, const char* fmt, va_list args) {
    return vsnprintf(out, capacity, fmt, args);
}

bool hg_platform_try(hg_body_t body, void* arg) {
    if (setjmp(failed) != 0) {
        return false;
    }
    body(arg);
    return true;
}

void hg_platform_fail(void) {
    longjmp(failed, 1);
}
