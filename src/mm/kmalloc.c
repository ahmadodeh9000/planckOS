#include "mm/kmalloc.h"
#include "lib/util.h"
#include "drivers/serial.h"
#include "mm/pmm.h"
#include "mm/paging.h"

static block_header* heap_start = NULL;

/* Big enough for a 1024x768x32 back buffer (3 MB) plus window buffers. */
#define HEAP_SIZE (16 * 1024 * 1024)

void heap_init() {
    /* The heap sits right after the kernel. It is told to the PMM so that
       pmm_alloc_page() never hands out a page the heap already owns. */
    uint32_t start = (uint32_t)kernel_end;
    uint32_t end   = start + HEAP_SIZE;

    if (end > PAGING_IDENTITY_MB * 1024 * 1024) {
        serial_printf("heap: does not fit in identity-mapped memory\n");
        for (;;) asm volatile("cli; hlt");
    }
    pmm_reserve_region(start, HEAP_SIZE);
    serial_printf("heap: %x - %x (%u KB)\n", start, end, HEAP_SIZE / 1024);

    heap_start       = (block_header*) start;
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