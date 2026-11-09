/*
 * demo.c - a small window used to try out the window manager: it shows the
 * last key typed and the last click position inside it.
 */
#include "apps/demo.h"
#include "gui/wm.h"
#include "gui/theme.h"
#include "gfx/font.h"
#include "mm/kmalloc.h"
#include "lib/string.h"

#define DEMO_WIDTH      320
#define DEMO_HEIGHT     160
#define TEXT_X          10
#define NUMBER_MAX      12
#define LINE_MAX        48

typedef struct {
    char last_key;
    int click_x, click_y;
} demo_state;

static void demo_paint(window_t* win) {
    demo_state* state = win->data;
    char line[LINE_MAX];
    char number[NUMBER_MAX];

    font_draw_text(&win->surface, TEXT_X, 10, "Type or click in this window.", THEME_TEXT);

    strcpy(line, "Last key: ");
    int len = strlen(line);
    line[len] = state->last_key ? state->last_key : '-';
    line[len + 1] = '\0';
    font_draw_text(&win->surface, TEXT_X, 10 + 2 * FONT_HEIGHT, line, THEME_TEXT);

    strcpy(line, "Last click: ");
    strcat(line, utoa(state->click_x, number, 10));
    strcat(line, ", ");
    strcat(line, utoa(state->click_y, number, 10));
    font_draw_text(&win->surface, TEXT_X, 10 + 3 * FONT_HEIGHT, line, THEME_TEXT);
}

static void demo_key(window_t* win, char key) {
    demo_state* state = win->data;
    state->last_key = key;
    window_invalidate(win);
}

static void demo_mouse(window_t* win, const event_t* e) {
    demo_state* state = win->data;
    if (e->type == EVENT_MOUSE_DOWN) {
        state->click_x = e->x;
        state->click_y = e->y;
        window_invalidate(win);
    }
}

static void demo_close(window_t* win) {
    kfree(win->data);
}

void demo_open(void) {
    demo_state* state = kmalloc(sizeof(demo_state));
    if (!state) {
        return;
    }
    window_t* win = window_create("Demo", DEMO_WIDTH, DEMO_HEIGHT);
    if (!win) {
        kfree(state);
        return;
    }
    memset(state, 0, sizeof(demo_state));
    win->data = state;
    win->on_paint = demo_paint;
    win->on_key = demo_key;
    win->on_mouse = demo_mouse;
    win->on_close = demo_close;
}
