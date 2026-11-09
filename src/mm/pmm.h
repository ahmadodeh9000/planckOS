/*
 * pmm.h - physical memory manager: hands out 4 KB physical pages.
 */
#ifndef pmm_h__
#define pmm_h__

#include "lib/common.h"
#include "arch/multiboot.h"

#define PAGE_SIZE   4096

/* Linker symbols from link.ld. Only their addresses mean anything. */
extern uint8_t kernel_start[];
extern uint8_t kernel_end[];

void pmm_init(multiboot_info_t* mbi);
void* pmm_alloc_page(void);
void pmm_free_page(void* addr);
void pmm_reserve_region(uint32_t start, uint32_t size);

/* Pages of RAM in total and in use (the heap counts as in use). */
void pmm_stats(uint32_t* total, uint32_t* used);

#endif
