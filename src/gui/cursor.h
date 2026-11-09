/*
 * cursor.h - the mouse pointer image.
 */
#ifndef cursor_h__
#define cursor_h__

#include "gfx/surface.h"

/* Draw the arrow with its tip at (x, y). */
void cursor_draw(surface_t* s, int x, int y);

#endif
