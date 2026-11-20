/*
 * iris_avatar.h - Iris's face, as small images drawn from a palette.
 */
#ifndef iris_avatar_h__
#define iris_avatar_h__

#include "gfx/surface.h"

#define IRIS_PORTRAIT_SIZE  48
#define IRIS_ICON_SIZE      16

typedef struct {
    int width;
    int height;
    const char* const* rows;    /* one string per row, one char per pixel */
} avatar_image_t;

extern const avatar_image_t iris_portrait;          /* 48x48, eyes open */
extern const avatar_image_t iris_portrait_blink;    /* 48x48, eyes closed */
extern const avatar_image_t iris_icon;              /* 16x16, the face */

/* Draw 'image' with its top-left at (x, y); '.' pixels are left alone. */
void avatar_draw(surface_t* s, int x, int y, const avatar_image_t* image);

#endif
