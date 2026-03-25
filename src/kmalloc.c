#include "kmalloc.h"
#include "util.h"

extern uint32_t kernel_end;

static block_header* heap_start = NULL;
#define HEAP_SIZE (4 * 1024 * 1024) /* 4 MB heap */

void heap_init() {

    printf("heap_start: %x\n", (uint32_t)&kernel_end);
    printf("heap_end:   %x\n", (uint32_t)&kernel_end + HEAP_SIZE);


    heap_start       = (block_header*) &kernel_end;
    heap_start->size = HEAP_SIZE - sizeof(block_header);
    heap_start->free = 1;
    heap_start->next = NULL;
}

void* kmalloc(size_t size) {

    size = (size + 7) & ~7;

    block_header* current = heap_start;

    while (current != NULL) {
        if (current->free && current->size >= size) {

          
            if (current->size >= size + sizeof(block_header) + 8) {
                block_header* next_block =
                    (block_header*)((char*)(current + 1) + size);

                next_block->size = current->size - size - sizeof(block_header);
                next_block->free = 1;
                next_block->next = current->next;

                current->size = size;
                current->next = next_block;
            }

            current->free = 0;
            return (void*)(current + 1);
        }

        current = current->next;
    }

    printf("kmalloc out of memory!\n");
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) return;

    block_header *block = ((block_header *)ptr) - 1;
    block->free = 1;

    
    while (block->next && block->next->free) {
        block->size += sizeof(block_header) + block->next->size;
        block->next  = block->next->next;
    }

   
}