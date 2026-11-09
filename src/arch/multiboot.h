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
#define MULTIBOOT_FLAG_FRAMEBUFFER  (1 << 12)   /* framebuffer_* fields valid  */
#define MULTIBOOT_MEMORY_AVAILABLE  1           /* mmap entry type for usable RAM */

#define MULTIBOOT_FRAMEBUFFER_INDEXED   0
#define MULTIBOOT_FRAMEBUFFER_RGB       1
#define MULTIBOOT_FRAMEBUFFER_TEXT      2       /* EGA text mode, no pixels */

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
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;

    /* Valid when MULTIBOOT_FLAG_FRAMEBUFFER is set. */
    uint64_t framebuffer_addr;  /* physical address of pixel (0, 0) */
    uint32_t framebuffer_pitch; /* bytes per row, may exceed width * bpp / 8 */
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;  /* MULTIBOOT_FRAMEBUFFER_* */

    /* GRUB declares the color info below as a union that also holds a
       32-bit palette address, so the compiler aligns it to 4 bytes. That
       leaves two unused bytes here. */
    uint8_t  color_info_padding[2];

    /* For MULTIBOOT_FRAMEBUFFER_RGB: bit position and size of each color. */
    uint8_t  red_field_position;
    uint8_t  red_mask_size;
    uint8_t  green_field_position;
    uint8_t  green_mask_size;
    uint8_t  blue_field_position;
    uint8_t  blue_mask_size;
} __attribute__((packed)) multiboot_info_t;

/* One memory map entry. 'size' does not count the size field itself. */
typedef struct {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) mmap_entry_t;

#endif
