/*
 * pmm.c - bitmap allocator for physical pages.
 *
 * One bit per 4 KB page: 1 = used, 0 = free. Everything starts out used.
 * Only RAM that GRUB's memory map calls available is freed, and then the
 * low 1 MB and the kernel image are reserved again. The heap reserves its
 * own range with pmm_reserve_region() (see kmalloc.c).
 */
#include "mm/pmm.h"
#include "drivers/serial.h"
#include "lib/string.h"

#define BITS_PER_WORD       32
#define MAX_PAGES           (1024 * 1024)               /* 4 GB / 4 KB */
#define BITMAP_WORDS        (MAX_PAGES / BITS_PER_WORD)
#define WORD_ALL_USED       0xFFFFFFFF
#define LOW_MEMORY_END      0x100000                    /* 1 MB: BIOS, VGA, ROMs */
#define ADDRESS_SPACE_END   0x100000000ULL              /* 4 GB, no PAE */
#define KB                  1024

static uint32_t bitmap[BITMAP_WORDS];
static uint32_t total_pages = 0;
static uint32_t used_pages = 0;

static void bitmap_set(uint32_t page) {
    bitmap[page / BITS_PER_WORD] |= (1u << (page % BITS_PER_WORD));
}

static void bitmap_clear(uint32_t page) {
    bitmap[page / BITS_PER_WORD] &= ~(1u << (page % BITS_PER_WORD));
}

static bool bitmap_test(uint32_t page) {
    return bitmap[page / BITS_PER_WORD] & (1u << (page % BITS_PER_WORD));
}

static void pmm_free_region(uint32_t first_page, uint32_t page_count) {
    for (uint32_t page = first_page; page < first_page + page_count; ++page) {
        if (page < total_pages && bitmap_test(page)) {
            bitmap_clear(page);
            used_pages--;
        }
    }
}

/* Free every page that GRUB's memory map reports as available RAM. */
static void pmm_free_available_ram(multiboot_info_t* mbi) {
    uint32_t entry_addr = mbi->mmap_addr;
    uint32_t mmap_end   = mbi->mmap_addr + mbi->mmap_length;

    while (entry_addr < mmap_end) {
        mmap_entry_t* entry = (mmap_entry_t*)entry_addr;

        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE && entry->addr < ADDRESS_SPACE_END) {
            pmm_free_region(entry->addr / PAGE_SIZE, entry->len / PAGE_SIZE);
        }
        entry_addr += entry->size + sizeof(entry->size);
    }
}

void pmm_reserve_region(uint32_t start, uint32_t size) {
    uint32_t first = start / PAGE_SIZE;
    uint32_t last  = (start + size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint32_t page = first; page < last && page < total_pages; ++page) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            used_pages++;
        }
    }
}

void pmm_init(multiboot_info_t* mbi) {
    if (!(mbi->flags & MULTIBOOT_FLAG_MMAP)) {
        serial_printf("pmm: GRUB gave no memory map\n");
        return;
    }

    total_pages = (mbi->mem_upper + LOW_MEMORY_END / KB) * KB / PAGE_SIZE;
    memset(bitmap, 0xFF, sizeof(bitmap));
    used_pages = total_pages;

    pmm_free_available_ram(mbi);

    /* GRUB may call parts of the first 1 MB free, but BIOS data and the VGA
       buffer live there. */
    pmm_reserve_region(0, LOW_MEMORY_END);
    pmm_reserve_region((uint32_t)kernel_start, (uint32_t)kernel_end - (uint32_t)kernel_start);

    serial_printf("pmm: %u pages total, %u used\n", total_pages, used_pages);
}

void* pmm_alloc_page(void) {
    /* Round up so the pages in a last, partly used bitmap word are found. */
    uint32_t words = (total_pages + BITS_PER_WORD - 1) / BITS_PER_WORD;

    for (uint32_t i = 0; i < words; ++i) {
        if (bitmap[i] == WORD_ALL_USED) {
            continue;
        }
        for (uint32_t bit = 0; bit < BITS_PER_WORD; ++bit) {
            uint32_t page = i * BITS_PER_WORD + bit;

            if (page < total_pages && !bitmap_test(page)) {
                bitmap_set(page);
                used_pages++;
                return (void*)(page * PAGE_SIZE);
            }
        }
    }

    serial_printf("pmm: out of memory\n");
    return NULL;
}

void pmm_stats(uint32_t* total, uint32_t* used) {
    *total = total_pages;
    *used = used_pages;
}

void pmm_free_page(void* addr) {
    pmm_free_region((uint32_t)addr / PAGE_SIZE, 1);
}
