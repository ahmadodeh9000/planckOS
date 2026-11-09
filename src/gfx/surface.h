/*
 * surface.h - a block of 32-bit pixels you can draw on.
 *
 * The screen's back buffer is a surface (see fb.h), and later each window
 * gets its own. Coordinates are relative to the surface's top-left corner,
 * and everything is clipped to the surface, so callers may draw partly
 * outside it.
 */
#ifndef surface_h__
#define surface_h__

#include "lib/common.h"

/* Pixels are 32-bit 0x00RRGGBB. */
#define RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

typedef struct {
    uint32_t* pixels;   /* width * height, row after row */
    int width;
    int height;
} surface_t;

void surface_put_pixel(surface_t* s, int x, int y, uint32_t color);
void surface_fill_rect(surface_t* s, int x, int y, int w, int h, uint32_t color);

/* Copy all of 'src' onto 'dst' with its top-left corner at (x, y). */
void surface_blit(surface_t* dst, int x, int y, const surface_t* src);

#endif
