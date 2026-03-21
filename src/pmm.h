#ifndef pmm_h__
#define pmm_h__

#include "common.h"

// The full multiboot info structure GRUB gives us
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;    // KB below 1MB
    uint32_t mem_upper;    // KB above 1MB
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;  // byte size of the mmap array
    uint32_t mmap_addr;    // address of the mmap array
} __attribute__((packed)) multiboot_info_t;

// One entry in the memory map array
typedef struct {
    uint32_t size;   // size of this entry (NOT including this size field)
    uint64_t addr;   // start address
    uint64_t len;    // length in bytes
    uint32_t type;   // 1 = free, 2 = reserved
} __attribute__((packed)) mmap_entry_t;

void pmm_init(multiboot_info_t *mbi);
void *pmm_alloc_page();
void pmm_free_page(void* addr);

#endif