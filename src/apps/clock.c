

#include "clock.h"
#include "gui/wm.h"
#include "gfx/surface.h"
#include "gui/widget.h"
#include "drivers/rtc.h"

#define CLOCK_WIDTH     300
#define CLOCK_HEIGHT    300
#define MARGIN          16
#define CLOCK_TITLE     "Clock"
#define SIN_SCALE       1000    /* the sine table is sin * 1000 */
#define CLOCK_THEME     0xFFFF00
#define THEME_TEXT      0x000000


// drawing hands
static const int quarter_sin[16] = {
    0, 105, 208, 309, 407, 500, 588, 669,
    743, 809, 866, 914, 951, 978, 995, 1000
};

/* sin of clock position i (0..59), scaled by 1000 */
static int clock_sin(int i) {
    i %= 60;
    if (i < 15) return  quarter_sin[i];          /*   0..90  */
    if (i < 30) return  quarter_sin[30 - i];     /*  90..180 */
    if (i < 45) return -quarter_sin[i - 30];     /* 180..270 */
    return -quarter_sin[60 - i];                 /* 270..360 */
}


/* cos is sin shifted by 90 degrees = 15 positions */
static int clock_cos(int i) {
    return clock_sin(i + 15);
}

/* drawing hours mark*/
static void draw_hours_mark(window_t* win, int cx, int cy, int r) {
    for (int i = 0; i < 60; i += 5) {
        int x0 = cx + (r * 85 / 100) * clock_sin(i) / SIN_SCALE;
        int y0 = cy - (r * 85 / 100) * clock_cos(i) / SIN_SCALE;
        int x1 = cx + (r - 4) * clock_sin(i) / SIN_SCALE;
        int y1 = cy - (r - 4) * clock_cos(i) / SIN_SCALE;
        surface_draw_line(&win->surface, x0, y0, x1, y1, THEME_TEXT);
    }
}

static void draw_hand(surface_t* s, int cx, int cy, int pos, int length, uint32_t color) {
    int tip_x = cx + length * clock_sin(pos) / 1000;
    int tip_y = cy - length * clock_cos(pos) / 1000;
    surface_draw_line(s, cx, cy, tip_x, tip_y, color);
}

static void draw_thick_hand(surface_t* s, int cx, int cy, int pos, int length, uint32_t color) {
    draw_hand(s, cx,     cy,     pos, length, color);
    draw_hand(s, cx + 1, cy,     pos, length, color);
    draw_hand(s, cx,     cy + 1, pos, length, color);
}


// drawing a disc
static void draw_clock_disk(surface_t* s, int cx, int cy, int r, uint32_t color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r) {
                surface_put_pixel(s,x + cx,y + cy,color);
            }
        }
    }
}

// get rtc time
void get_clock_time(int* hp, int* mp, int* sp) {
    rtc_time_t now;
    rtc_read(&now);
    
    *hp = (now.hour % 12) * 5 + now.minute / 12;
    *mp = now.minute;
    *sp = now.second;
}

static void clock_paint(window_t* win) {
    int cx = win->surface.width / 2;
    int cy = win->surface.height / 2;
    int r  = cx < cy ? cx - MARGIN : cy - MARGIN;

    int hour_pos, minute_pos, second_pos;
    get_clock_time(&hour_pos, &minute_pos, &second_pos);

    draw_clock_disk(&win->surface,cx,cy,r,CLOCK_THEME);
    draw_hours_mark(win,cx,cy,r);
    
    draw_thick_hand(&win->surface, cx, cy, hour_pos,   r * 50 / 100, THEME_TEXT);
    draw_thick_hand(&win->surface, cx + 1, cy, minute_pos, r * 75 / 100, THEME_TEXT);
    draw_hand(&win->surface, cx, cy, second_pos, r * 85 / 100, 0XFF0000);  /* red */
}

static void clock_timer(window_t* win) {
    window_invalidate(win);         /* repaint every second */
}


void clock_open(void) {
    window_t* win = window_create(CLOCK_TITLE, CLOCK_WIDTH, CLOCK_HEIGHT);
    if (!win) {
        return;
    }
    win->on_paint = clock_paint;
    win->on_timer = clock_timer;
}