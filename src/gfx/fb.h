/*
 * fb.h - linear framebuffer with a back buffer.
 *
 * All drawing goes into an off-screen back buffer in RAM; fb_present()
 * copies it to the screen in one go. That avoids flicker (the screen never
 * shows a half-drawn frame) and is faster, because reading or writing
 * video memory is much slower than RAM.
 *
 * Coordinates are in pixels with (0, 0) at the top-left. Everything is
 * clipped to the screen, so callers may draw partly off-screen.
 */
#ifndef fb_h__
#define fb_h__

#include "lib/common.h"
#include "arch/multiboot.h"

/* Pixels are 32-bit 0x00RRGGBB. */
#define FB_RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

/* Map the framebuffer GRUB set up and allocate the back buffer. Returns
   false (and leaves graphics off) unless it is a 32-bit RGB mode. */
bool fb_init(multiboot_info_t* mbi);

uint32_t fb_width(void);
uint32_t fb_height(void);

void fb_put_pixel(int x, int y, uint32_t color);
void fb_fill_rect(int x, int y, int w, int h, uint32_t color);

/* Copy a w x h block of pixels to (x, y). 'stride' is the number of pixels
   per row in 'pixels' (usually w). */
void fb_blit(int x, int y, int w, int h, const uint32_t* pixels, int stride);

/* Copy the back buffer to the screen. */
void fb_present(void);

#endif
