/*
 * platform_kernel.c - the Higgson compiler's platform layer inside planckOS.
 *
 * Memory is the kernel heap, text formatting is ksnprintf, and an error
 * is a longjmp back to hg_platform_try: there's no exit() in a kernel, and
 * the compiler must give control back to the higgson command (which keeps
 * running, as does everything else) after any error.
 */
#include "higgson/platform.h"
#include "arch/jump.h"
#include "lib/format.h"
#include "mm/kmalloc.h"

static jmp_buf failed;      /* where hg_platform_fail jumps to */

void* hg_platform_alloc(uint32_t size) {
    return kmalloc(size);
}

void hg_platform_free(void* block) {
    kfree(block);
}

int hg_platform_vformat(char* out, uint32_t capacity, const char* fmt, va_list args) {
    return kvsnprintf(out, capacity, fmt, args);
}

/* Nothing here changes between setjmp and a longjmp back to it, so no
   variable can be left with a stale value by the jump. */
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
