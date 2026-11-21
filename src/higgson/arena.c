/*
 * arena.c - the Higgson compiler's memory (see arena.h).
 */
#include "higgson/arena.h"

#define ARENA_ALIGN 8       /* enough for any value the compiler stores */

bool hg_arena_init(hg_arena_t* arena, uint32_t capacity) {
    arena->base = hg_platform_alloc(capacity);
    arena->used = 0;
    arena->capacity = arena->base ? capacity : 0;
    return arena->base != NULL;
}

void* hg_arena_alloc(hg_arena_t* arena, uint32_t size) {
    uint32_t start = (arena->used + ARENA_ALIGN - 1) & ~(uint32_t)(ARENA_ALIGN - 1);

    /* Written so that a huge 'size' can't wrap around and look small. */
    if (start > arena->capacity || size > arena->capacity - start) {
        return NULL;
    }
    arena->used = start + size;
    return arena->base + start;
}

void hg_arena_free(hg_arena_t* arena) {
    if (arena->base) {
        hg_platform_free(arena->base);
    }
    arena->base = NULL;
    arena->used = 0;
    arena->capacity = 0;
}
