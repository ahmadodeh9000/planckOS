#ifndef kmalloc_h__
#define kmalloc_h__

#include "common.h"

typedef struct block_header{
    size_t size;
    int32_t free;
    struct block_header* next;

} block_header;


void heap_init();
void* kmalloc(size_t size);
void kfree(void* ptr);

#endif