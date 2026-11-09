/*
 * cursor.c - draws the mouse pointer.
 *
 * The arrow is stored as text so it is easy to see and edit: 'X' is the
 * outline, '.' the fill, and ' ' is transparent (the screen shows through).
 * It is drawn last, on top of everything else, every time the screen is
 * redrawn.
 */
#include "gui/cursor.h"

#define CURSOR_HEIGHT   19
#define COLOR_OUTLINE   RGB(0xFF, 0xFF, 0xFF)
#define COLOR_FILL      RGB(0x00, 0x00, 0x00)

static const char* const arrow[CURSOR_HEIGHT] = {
    "X",
    "XX",
    "X.X",
    "X..X",
    "X...X",
    "X....X",
    "X.....X",
    "X......X",
    "X.......X",
    "X........X",
    "X.........X",
    "X......XXXXX",
    "X...X..X",
    "X..XX..X",
    "X.X  X..X",
    "XX   X..X",
    "X     X..X",
    "      X..X",
    "       XX",
};

void cursor_draw(surface_t* s, int x, int y) {
    for (int row = 0; row < CURSOR_HEIGHT; row++) {
        for (int col = 0; arrow[row][col] != '\0'; col++) {
            char pixel = arrow[row][col];
            if (pixel == 'X') {
                surface_put_pixel(s, x + col, y + row, COLOR_OUTLINE);
            } else if (pixel == '.') {
                surface_put_pixel(s, x + col, y + row, COLOR_FILL);
            }
        }
    }
}
