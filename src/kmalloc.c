#include "kmalloc.h"
#include "util.h"

extern uint32_t kernel_end;

static block_header* heap_start = NULL;
#define HEAP_SIZE (1024 * 1024) /* 1 MB of heap*/

void heap_init() {
   heap_start       = (block_header*) &kernel_end;
   heap_start->size = HEAP_SIZE - sizeof(block_header);
   heap_start->free = 1;
   heap_start->next = NULL;
}


void* kmalloc(size_t size) {
    block_header* current = heap_start;


    while (current != NULL) {
        if (current->free == 1&& current->size >= size) {
            
            block_header* next_block = (block_header*)((char*)(current + 1) + size);
            next_block->size = current->size - size - sizeof(block_header);
            next_block->free = 1;
            next_block->next = current->next;

            current->size = size;
            current->next = next_block;
            current->free = 0;


            return (void*)(current + 1);
        }

        current = current->next;
    }

    printf("kmalloc out of memory !\n");
    return NULL;

}

void kfree(void* ptr) {
    block_header* current = ((block_header*)ptr) - 1;
    current->free = 1;
}

