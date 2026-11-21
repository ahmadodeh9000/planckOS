/*
 * platform_kernel.c - the Higgson compiler's platform layer inside planckOS.
 *
 * Memory is the kernel heap, text formatting is ksnprintf, and an error
 * is a longjmp back to hg_platform_try: there's no exit() in a kernel, and
 * the compiler must give control back to the higgson command (which keeps
 * running, as does everything else) after any error.
 *
 * The compiler also runs on a stack of its own. Its recursive parser needs
 * about 700 bytes for every level of parentheses, and the kernel's stack
 * is only 8 KB: sixteen levels would overflow it into whatever lies below.
 */
#include "higgson/platform.h"
#include "arch/jump.h"
#include "lib/format.h"
#include "mm/kmalloc.h"

#define COMPILER_STACK_SIZE (32 * 1024)

static jmp_buf failed;      /* where hg_platform_fail jumps to */
static uint8_t compiler_stack[COMPILER_STACK_SIZE] __attribute__((aligned(16)));

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
    call_on_stack(body, arg, compiler_stack + COMPILER_STACK_SIZE);
    return true;
}

void hg_platform_fail(void) {
    longjmp(failed, 1);
}
