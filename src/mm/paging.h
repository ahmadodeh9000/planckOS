/*
 * paging.h - turn on paging and map extra pages (e.g. device memory).
 */
#ifndef paging_h__
#define paging_h__

#include "lib/common.h"

/* Physical memory below this is identity-mapped by paging_init(). */
#define PAGING_IDENTITY_MB  64

/* Page table entry flags */
#define PAGE_PRESENT        (1 << 0)
#define PAGE_WRITABLE       (1 << 1)
#define PAGE_USER           (1 << 2)
#define PAGE_WRITE_THROUGH  (1 << 3)
#define PAGE_CACHE_DISABLE  (1 << 4)

void paging_init(void);

/* Map one 4 KB page. Returns false if a page table couldn't be allocated. */
bool paging_map(uint32_t virt, uint32_t phys, uint32_t flags);

#endif
