/*
 * fb.c - linear framebuffer driver.
 *
 * GRUB switches the video card into a graphics mode (see boot.s) and tells
 * us where its memory is. The framebuffer usually lives far above RAM
 * (QEMU: 0xFD000000), outside our 64 MB identity map, so fb_init() maps it
 * page by page with paging_map(). Drawing happens in a back buffer on the
 * heap; fb_present() copies it to video memory row by row, because a
 * screen row ('pitch' bytes) can be longer than width * 4.
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
static uint32_t* back_buffer;       /* width * height pixels  */
static uint32_t  screen_width;
static uint32_t  screen_height;
static uint32_t  screen_pitch;      /* bytes per screen row   */

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

    uint32_t phys = (uint32_t)mbi->framebuffer_addr;
    screen_width  = mbi->framebuffer_width;
    screen_height = mbi->framebuffer_height;
    screen_pitch  = mbi->framebuffer_pitch;

    if (!fb_map(phys, screen_pitch * screen_height)) {
        serial_printf("fb: could not map video memory\n");
        return false;
    }
    back_buffer = kmalloc(screen_width * screen_height * FB_BYTES_PER_PIXEL);
    if (!back_buffer) {
        serial_printf("fb: no memory for the back buffer\n");
        return false;
    }
    front_buffer = (uint8_t*)phys;

    serial_printf("fb: %ux%u, pitch %u, at %x\n", screen_width, screen_height, screen_pitch, phys);
    return true;
}

uint32_t fb_width(void) {
    return screen_width;
}

uint32_t fb_height(void) {
    return screen_height;
}

void fb_put_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && y >= 0 && (uint32_t)x < screen_width && (uint32_t)y < screen_height) {
        back_buffer[y * screen_width + x] = color;
    }
}

/*
 * Shrink the rectangle (x, y, w, h) to the part that is on screen. Also
 * moves (src_x, src_y) by the amount cut off the left/top, so blits stay
 * aligned with their source. Returns false if nothing is left.
 */
static bool fb_clip(int* x, int* y, int* w, int* h, int* src_x, int* src_y) {
    if (*x < 0) {
        *w += *x;
        *src_x -= *x;
        *x = 0;
    }
    if (*y < 0) {
        *h += *y;
        *src_y -= *y;
        *y = 0;
    }
    if (*x + *w > (int)screen_width) {
        *w = screen_width - *x;
    }
    if (*y + *h > (int)screen_height) {
        *h = screen_height - *y;
    }
    return *w > 0 && *h > 0;
}

void fb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    int unused_x = 0;
    int unused_y = 0;

    if (!fb_clip(&x, &y, &w, &h, &unused_x, &unused_y)) {
        return;
    }
    for (int row = 0; row < h; row++) {
        memset32(&back_buffer[(y + row) * screen_width + x], color, w);
    }
}

void fb_blit(int x, int y, int w, int h, const uint32_t* pixels, int stride) {
    int src_x = 0;
    int src_y = 0;

    if (!fb_clip(&x, &y, &w, &h, &src_x, &src_y)) {
        return;
    }
    for (int row = 0; row < h; row++) {
        const uint32_t* src = &pixels[(src_y + row) * stride + src_x];
        memcpy(&back_buffer[(y + row) * screen_width + x], src, w * FB_BYTES_PER_PIXEL);
    }
}

void fb_present(void) {
    uint32_t row_bytes = screen_width * FB_BYTES_PER_PIXEL;

    for (uint32_t row = 0; row < screen_height; row++) {
        memcpy(front_buffer + row * screen_pitch, &back_buffer[row * screen_width], row_bytes);
    }
}
