/*
 * surface.c - pixel drawing on surfaces: pixels, rectangles and blits.
 */
#include "gfx/surface.h"
#include "lib/string.h"

#define BYTES_PER_PIXEL 4

/*
 * Shrink 'r' to the part inside a width x height area. 'src_x'/'src_y'
 * move by whatever is cut off the left/top, so a blit stays lined up with
 * its source. Returns false if nothing is left.
 */
static bool clip_rect(rect_t* r, int width, int height, int* src_x, int* src_y) {
    if (r->x < 0) {
        r->w += r->x;
        *src_x -= r->x;
        r->x = 0;
    }
    if (r->y < 0) {
        r->h += r->y;
        *src_y -= r->y;
        r->y = 0;
    }
    if (r->x + r->w > width) {
        r->w = width - r->x;
    }
    if (r->y + r->h > height) {
        r->h = height - r->y;
    }
    return r->w > 0 && r->h > 0;
}

void surface_put_pixel(surface_t* s, int x, int y, uint32_t color) {
    if (x >= 0 && y >= 0 && x < s->width && y < s->height) {
        s->pixels[y * s->width + x] = color;
    }
}

void surface_fill_rect(surface_t* s, int x, int y, int w, int h, uint32_t color) {
    rect_t r = { x, y, w, h };
    int unused_x = 0;
    int unused_y = 0;

    if (!clip_rect(&r, s->width, s->height, &unused_x, &unused_y)) {
        return;
    }
    for (int row = 0; row < r.h; row++) {
        memset32(&s->pixels[(r.y + row) * s->width + r.x], color, r.w);
    }
}

void surface_draw_rect(surface_t* s, int x, int y, int w, int h, uint32_t color) {
    surface_fill_rect(s, x, y, w, 1, color);
    surface_fill_rect(s, x, y + h - 1, w, 1, color);
    surface_fill_rect(s, x, y, 1, h, color);
    surface_fill_rect(s, x + w - 1, y, 1, h, color);
}

void surface_blit(surface_t* dst, int x, int y, const surface_t* src) {
    rect_t r = { x, y, src->width, src->height };
    int src_x = 0;
    int src_y = 0;

    if (!clip_rect(&r, dst->width, dst->height, &src_x, &src_y)) {
        return;
    }
    for (int row = 0; row < r.h; row++) {
        uint32_t* to = &dst->pixels[(r.y + row) * dst->width + r.x];
        const uint32_t* from = &src->pixels[(src_y + row) * src->width + src_x];
        memcpy(to, from, r.w * BYTES_PER_PIXEL);
    }
}
