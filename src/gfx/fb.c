/*
 * fb.c - linear framebuffer driver.
 *
 * GRUB switches the video card into a graphics mode (see boot.s) and tells
 * us where its memory is. The framebuffer usually lives far above RAM
 * (QEMU: 0xFD000000), outside our 64 MB identity map, so fb_init() maps it
 * page by page with paging_map(). Drawing happens in a back buffer on the
 * heap; fb_present() copies it to video memory row by row, because a
 * screen row ('pitch' bytes) can be longer than width * 4. The drawing
 * functions themselves are in surface.c.
 */
#include "gfx/fb.h"
#include "mm/kmalloc.h"
#include "mm/paging.h"
#include "mm/pmm.h"
#include "drivers/serial.h"
#include "lib/string.h"

#define FB_BYTES_PER_PIXEL  4
#define FB_BITS_PER_PIXEL   32
#define FB_RED_POSITION     16
#define FB_GREEN_POSITION   8
#define FB_BLUE_POSITION    0
#define ADDRESS_SPACE_END   0x100000000ULL

static uint8_t*  front_buffer;      /* video memory           */
static uint32_t  screen_pitch;      /* bytes per screen row   */
static surface_t back_buffer;

/* We only draw 0x00RRGGBB pixels, so accept nothing else. */
static bool fb_mode_supported(multiboot_info_t* mbi) {
    return (mbi->flags & MULTIBOOT_FLAG_FRAMEBUFFER) &&
           mbi->framebuffer_type == MULTIBOOT_FRAMEBUFFER_RGB &&
           mbi->framebuffer_bpp == FB_BITS_PER_PIXEL &&
           mbi->red_field_position == FB_RED_POSITION &&
           mbi->green_field_position == FB_GREEN_POSITION &&
           mbi->blue_field_position == FB_BLUE_POSITION &&
           mbi->framebuffer_addr + (uint64_t)mbi->framebuffer_pitch *
               mbi->framebuffer_height <= ADDRESS_SPACE_END;
}

/* Identity-map the framebuffer's pages. */
static bool fb_map(uint32_t phys, uint32_t size) {
    for (uint32_t offset = 0; offset < size; offset += PAGE_SIZE) {
        /* Write-through: writes go straight to the card instead of
           lingering in the CPU cache. */
        if (!paging_map(phys + offset, phys + offset, PAGE_WRITABLE | PAGE_WRITE_THROUGH)) {
            return false;
        }
    }
    return true;
}

bool fb_init(multiboot_info_t* mbi) {
    if (!fb_mode_supported(mbi)) {
        serial_printf("fb: no 32-bit RGB framebuffer, staying in text mode\n");
        return false;
    }

    uint32_t phys   = (uint32_t)mbi->framebuffer_addr;
    uint32_t width  = mbi->framebuffer_width;
    uint32_t height = mbi->framebuffer_height;
    screen_pitch    = mbi->framebuffer_pitch;

    if (!fb_map(phys, screen_pitch * height)) {
        serial_printf("fb: could not map video memory\n");
        return false;
    }
    back_buffer.pixels = kmalloc(width * height * FB_BYTES_PER_PIXEL);
    back_buffer.width  = width;
    back_buffer.height = height;
    if (!back_buffer.pixels) {
        serial_printf("fb: no memory for the back buffer\n");
        return false;
    }
    front_buffer = (uint8_t*)phys;

    serial_printf("fb: %ux%u, pitch %u, at %x\n", width, height, screen_pitch, phys);
    return true;
}

surface_t* fb_back_buffer(void) {
    return &back_buffer;
}

void fb_present(void) {
    uint32_t row_bytes = back_buffer.width * FB_BYTES_PER_PIXEL;

    for (int row = 0; row < back_buffer.height; row++) {
        memcpy(front_buffer + row * screen_pitch, &back_buffer.pixels[row * back_buffer.width], row_bytes);
    }
}
