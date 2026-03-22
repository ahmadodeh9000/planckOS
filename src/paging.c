#include "paging.h"
#include "pmm.h"
#include "common.h"
#include "util.h"

#define PAGE_SIZE   4096
#define PAGE_PRESENT    (1 << 0)        /* page exist           */
#define PAGE_WRITABLE   (1 << 1)        /* page writable        */
#define PAGE_USER       (1 << 2)        /* user can access it   */

static uint32_t page_directory[1024]    __attribute__((aligned(4096)));
static uint32_t first_table[1024]       __attribute__((aligned(4096)));

void paging_init() {


    for (uint32_t i = 0; i < 1024; ++i)
        page_directory[i] = 0;


    for (uint32_t i = 0; i < 1024; ++i) {
        uint32_t physical_addr = i * PAGE_SIZE;
        first_table[i] = physical_addr | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    }


    page_directory[0] = (uint32_t)first_table | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

    asm volatile("mov %0, %%cr3" :: "r"(page_directory));

    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1 << 31);
    asm volatile("mov %0, %%cr0" :: "r"(cr0));

    printf("paging works\n");
}