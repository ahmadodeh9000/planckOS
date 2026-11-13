/*
 * click.c - the C version of click.asm: click the window to switch the
 * box between red and blue.
 *
 * _start opens the window, registers the two callbacks and returns. From
 * then on the window manager calls paint() and click(). click() only
 * changes 'color' and asks for a repaint; paint() does the drawing,
 * because drawing calls only work inside paint.
 */
#include "planck.h"

#define WINDOW_WIDTH    200
#define WINDOW_HEIGHT   100
#define BOX_X           10
#define BOX_Y           10
#define BOX_WIDTH       80
#define BOX_HEIGHT      32
#define RED             0x00FF0000
#define BLUE            0x000000FF

static uint32_t color = RED;

/* paint(id): draw the box in the current color. */
static void paint(uint32_t id) {
    sys_win_rect(id, BOX_X, BOX_Y, BOX_WIDTH, BOX_HEIGHT, color);
}

/* click(id, x, y): flip the color, then ask for a repaint. */
static void click(uint32_t id, int x, int y) {
    (void)x;
    (void)y;
    color = (color == RED) ? BLUE : RED;
    sys_win_invalidate(id);
}

int _start(void) {
    uint32_t id = sys_win_create("Click me", WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!id) {
        return 1;
    }
    sys_win_on(id, WIN_EVENT_PAINT, paint);
    sys_win_on(id, WIN_EVENT_MOUSE, click);
    return 0;
}
