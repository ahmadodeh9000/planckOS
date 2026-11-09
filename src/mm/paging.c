/*
 * paging.c - x86 two-level paging (no PAE).
 *
 * The first PAGING_IDENTITY_MB of physical memory are identity-mapped
 * (virtual address == physical address). That range covers the kernel, the
 * heap and the page tables that paging_map() allocates later, so kernel
 * pointers keep working once paging is on. Devices above that range, like
 * the linear framebuffer, are added with paging_map().
 */
#include "mm/paging.h"
#include "mm/pmm.h"
#include "drivers/serial.h"

#define ENTRIES_PER_TABLE   1024
#define TABLE_SPAN          (PAGE_SIZE * ENTRIES_PER_TABLE)    /* 4 MB */
#define IDENTITY_TABLES     (PAGING_IDENTITY_MB * 1024 * 1024 / TABLE_SPAN)

#define CR0_PAGING          (1u << 31)
#define ENTRY_ADDR_MASK     0xFFFFF000

static uint32_t page_directory[ENTRIES_PER_TABLE] __attribute__((aligned(PAGE_SIZE)));
static uint32_t identity_tables[IDENTITY_TABLES][ENTRIES_PER_TABLE]
    __attribute__((aligned(PAGE_SIZE)));

static void paging_map_identity(void) {
    for (uint32_t t = 0; t < IDENTITY_TABLES; ++t) {
        for (uint32_t i = 0; i < ENTRIES_PER_TABLE; ++i) {
            uint32_t phys = t * TABLE_SPAN + i * PAGE_SIZE;
            identity_tables[t][i] = phys | PAGE_PRESENT | PAGE_WRITABLE;
        }
        page_directory[t] = (uint32_t)identity_tables[t] | PAGE_PRESENT | PAGE_WRITABLE;
    }
}

/*
 * Return the page table that covers 'virt', allocating it if needed.
 * New tables come from the PMM. PMM pages are only reachable through the
 * identity map, so a table above that range can't be written and counts
 * as a failure.
 */
static uint32_t* paging_get_table(uint32_t virt) {
    uint32_t dir_index = virt / TABLE_SPAN;

    if (page_directory[dir_index] & PAGE_PRESENT) {
        return (uint32_t*)(page_directory[dir_index] & ENTRY_ADDR_MASK);
    }

    uint32_t* table = pmm_alloc_page();
    if (!table || (uint32_t)table >= PAGING_IDENTITY_MB * 1024 * 1024) {
        serial_printf("paging: no identity-mapped page for a new table\n");
        return NULL;
    }
    for (uint32_t i = 0; i < ENTRIES_PER_TABLE; ++i) {
        table[i] = 0;
    }
    page_directory[dir_index] = (uint32_t)table | PAGE_PRESENT | PAGE_WRITABLE;
    return table;
}

bool paging_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t* table = paging_get_table(virt);
    if (!table) {
        return false;
    }

    uint32_t table_index = (virt / PAGE_SIZE) % ENTRIES_PER_TABLE;
    table[table_index] = (phys & ENTRY_ADDR_MASK) | flags | PAGE_PRESENT;

    /* The CPU may still have the old translation cached in its TLB. */
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
    return true;
}

void paging_init(void) {
    paging_map_identity();

    asm volatile("mov %0, %%cr3" :: "r"(page_directory));

    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= CR0_PAGING;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));

    serial_printf("paging: enabled, first %u MB identity-mapped\n", PAGING_IDENTITY_MB);
}
