/*
 * kmalloc.c - first-fit heap allocator.
 *
 * The heap is one 16 MB region right after the kernel image. It is a linked
 * list of chunks, each starting with a block_header. kmalloc splits a free
 * chunk when there is room left over, and kfree merges a freed chunk with
 * the free chunks after it.
 */
#include "mm/kmalloc.h"
#include "mm/pmm.h"
#include "mm/paging.h"
#include "drivers/serial.h"

/* Big enough for a 1024x768x32 back buffer (3 MB) plus window buffers. */
#define HEAP_SIZE           (16 * 1024 * 1024)
#define HEAP_ALIGN          8
#define MIN_SPLIT_PAYLOAD   8   /* don't create leftover chunks smaller than this */

typedef struct block_header {
    size_t size;                /* payload bytes, excluding this header */
    int32_t free;
    struct block_header* next;
} block_header;

static block_header* heap_start = NULL;

void heap_init(void) {
    /* The heap is reserved in the PMM so pmm_alloc_page() never hands out
       a page the heap already owns. */
    uint32_t start = (uint32_t)kernel_end;
    uint32_t end   = start + HEAP_SIZE;

    if (end > PAGING_IDENTITY_MB * 1024 * 1024) {
        serial_printf("heap: does not fit in identity-mapped memory\n");
        for (;;) asm volatile("cli; hlt");
    }
    pmm_reserve_region(start, HEAP_SIZE);
    serial_printf("heap: %x - %x (%u KB)\n", start, end, HEAP_SIZE / 1024);

    heap_start       = (block_header*)start;
    heap_start->size = HEAP_SIZE - sizeof(block_header);
    heap_start->free = 1;
    heap_start->next = NULL;
}

/* Cut 'block' down to 'size' bytes if the rest can hold another chunk. */
static void heap_split(block_header* block, size_t size) {
    if (block->size < size + sizeof(block_header) + MIN_SPLIT_PAYLOAD) {
        return;
    }

    block_header* rest = (block_header*)((uint8_t*)(block + 1) + size);
    rest->size = block->size - size - sizeof(block_header);
    rest->free = 1;
    rest->next = block->next;

    block->size = size;
    block->next = rest;
}

void* kmalloc(size_t size) {
    size = (size + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);

    for (block_header* block = heap_start; block != NULL; block = block->next) {
        if (block->free && block->size >= size) {
            heap_split(block, size);
            block->free = 0;
            return block + 1;
        }
    }

    serial_printf("kmalloc: out of memory (%u bytes)\n", size);
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) {
        return;
    }

    block_header* block = (block_header*)ptr - 1;
    block->free = 1;

    while (block->next && block->next->free) {
        block->size += sizeof(block_header) + block->next->size;
        block->next  = block->next->next;
    }
}

void heap_stats(uint32_t* used, uint32_t* free) {
    *used = 0;
    *free = 0;
    for (block_header* block = heap_start; block != NULL; block = block->next) {
        if (block->free) {
            *free += block->size;
        } else {
            *used += block->size;
        }
    }
}
