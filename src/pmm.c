#include "pmm.h"
#include "util.h"

/* constants for paging*/

#define PAGE_SIZE   4096
#define MAX_PAGES   (1024 * 1024)       // 4GB of RAM
#define BITMAP_SIZE (MAX_PAGES / 32)    

/* bitmaps */
static uint32_t bitmap[BITMAP_SIZE];
static uint32_t total_pages = 0;
static uint32_t used_pages = 0;


/* |-------------BITMAPS HELPERS-----------------| */

/* mark the page as used */
static void bitmap_set(uint32_t page) {
    bitmap[page/32] |= (1 << (page % 32)); 
}
/* set the page free */
static void bitmap_clear(uint32_t page) {
    bitmap[page/32] &= ~(1 << (page % 32)); 
}
/* check if the page is used*/
static int32_t bitmap_test(uint32_t page) {     
    return bitmap[page/32] & (1 << (page % 32));
}


void pmm_init(multiboot_info_t *mbi) {
    if (!(mbi->flags & (1 << 6))) {
        printf("There are no memory map from grub\n");
        return;
    }

    // Total RAM in bytes = (mem_upper + 1024) KB converted to bytes
    // mem_upper is KB above 1MB, mem_lower is KB below 1MB (usually 640)

    uint32_t total_ram = (mbi->mem_upper + 1024) * 1024;
    total_pages = (total_ram / PAGE_SIZE);

    // marking everything as used for safty

    for (uint32_t i = 0; i < BITMAP_SIZE; ++i) {
        //          0xFFFFFFFF;
        bitmap[i] = 0xFFFFFFFF;
    }

    used_pages = total_pages;

    mmap_entry_t* entry = (mmap_entry_t*) mbi->mmap_addr;

    while ((uint32_t) entry < mbi->mmap_addr + mbi->mmap_length) {
        /* type 1 => free/ stable */
        if (entry->type == 1) { 
            uint32_t page_start = entry->addr / PAGE_SIZE;
            uint32_t page_count = entry->len / PAGE_SIZE;

            for (uint32_t i = 0; i < page_count ; ++i) {
                uint32_t page = page_start + i;

                if (page_start < total_pages && bitmap_test(page)) {
                    bitmap_clear(page);
                    used_pages--;
                }
            }
        }


        entry = (mmap_entry_t*)((uint32_t) entry + entry->size + sizeof(uint32_t));
    }


    // Step 3: mark the first 1MB as used (BIOS, VGA buffer, etc. live here)
    // Even if GRUB said it's free, we must never touch it
    uint32_t low_pages = 0x100000 / PAGE_SIZE;  // 0x100000 = 1MB

    uint32_t kstart = (uint32_t)&kernel_start / PAGE_SIZE;
    uint32_t kend   = (uint32_t)&kernel_end   / PAGE_SIZE + 1;

    for (uint32_t i = 0; i < low_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages++;
        }
    }

    printf("PMM: initialized\n");
}

void* pmm_alloc_page() {
    for (uint32_t i = 0; i < total_pages / 32; ++i) {
        if (0xFFFFFFFF == bitmap[i]) continue; /* used page skip it */

        for (uint32_t bit = 0; bit < 32; ++bit) {
            uint32_t page = i * 32 + bit;

            if (!bitmap_test(page)) {
                bitmap_set(page);
                used_pages++;

                return (void*)(page * PAGE_SIZE);
            }
        }
    }

    printf("PMM: out of memory!\n");
    return 0;  // null = failed
}

void pmm_free_page(void* addr) {
    uint32_t page = (uint32_t) addr / PAGE_SIZE;


    if (bitmap_test(page)) {
        bitmap_clear(page);
        used_pages--;
    }
}