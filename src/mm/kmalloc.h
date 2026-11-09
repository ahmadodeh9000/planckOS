/*
 * kmalloc.h - the kernel heap.
 */
#ifndef kmalloc_h__
#define kmalloc_h__

#include "lib/common.h"

void heap_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);

#endif
