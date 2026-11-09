/*
 * fb.h - linear framebuffer with a back buffer.
 *
 * All drawing goes into an off-screen back buffer in RAM (a surface_t);
 * fb_present() copies it to the screen in one go. That avoids flicker (the
 * screen never shows a half-drawn frame) and is faster, because video
 * memory is much slower to touch than RAM.
 */
#ifndef fb_h__
#define fb_h__

#include "lib/common.h"
#include "arch/multiboot.h"
#include "gfx/surface.h"

/* Map the framebuffer GRUB set up and allocate the back buffer. Returns
   false (and leaves graphics off) unless it is a 32-bit RGB mode. */
bool fb_init(multiboot_info_t* mbi);

/* The back buffer: draw here, then call fb_present(). */
surface_t* fb_back_buffer(void);

/* Copy the back buffer to the screen. */
void fb_present(void);

#endif
