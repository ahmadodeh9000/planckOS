/*
 * font.h - 8x16 bitmap font for printable ASCII.
 */
#ifndef font_h__
#define font_h__

#include "lib/common.h"
#include "gfx/surface.h"

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

/* Draw one character with its top-left corner at (x, y). Only the glyph's
   pixels are drawn; the background shows through. */
void font_draw_char(surface_t* s, int x, int y, char c, uint32_t color);

/* Draw a string on one line ('\n' is not handled). */
void font_draw_text(surface_t* s, int x, int y, const char* text, uint32_t color);

#endif
