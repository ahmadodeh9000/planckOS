/*
 * multiboot.h - the boot information GRUB passes to kmain (Multiboot 1).
 *
 * GRUB leaves a pointer to multiboot_info_t in ebx. Each field is only
 * valid if its bit in 'flags' is set.
 */
#ifndef multiboot_h__
#define multiboot_h__

#include "lib/common.h"

#define MULTIBOOT_BOOTLOADER_MAGIC  0x2BADB002
#define MULTIBOOT_FLAG_MMAP         (1 << 6)    /* mmap_addr/mmap_length valid */
#define MULTIBOOT_MEMORY_AVAILABLE  1           /* mmap entry type for usable RAM */

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;         /* KB below 1 MB */
    uint32_t mem_upper;         /* KB above 1 MB, up to the first hole */
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;       /* size of the mmap array in bytes */
    uint32_t mmap_addr;         /* physical address of the mmap array */
} __attribute__((packed)) multiboot_info_t;

/* One memory map entry. 'size' does not count the size field itself. */
typedef struct {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) mmap_entry_t;

#endif
