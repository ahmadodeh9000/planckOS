/*
 * arena.h - the Higgson compiler's memory: one block, handed out in pieces.
 *
 * Everything one compilation allocates (token texts, the parser, the
 * assembly it writes) comes from a single block that is freed in one go at
 * the end. Nothing is freed piece by piece, so nothing can leak, even when
 * an error stops the compiler halfway through.
 */
#ifndef higgson_arena_h__
#define higgson_arena_h__

#include "higgson/platform.h"

typedef struct {
    uint8_t* base;          /* the block, from hg_platform_alloc */
    uint32_t used;
    uint32_t capacity;
} hg_arena_t;

/* False if the platform has no block of that size. */
bool hg_arena_init(hg_arena_t* arena, uint32_t capacity);

/* 'size' bytes, aligned for any value; NULL when the block is full. */
void* hg_arena_alloc(hg_arena_t* arena, uint32_t size);

/* Gives the whole block back. Safe to call twice. */
void hg_arena_free(hg_arena_t* arena);

#endif
