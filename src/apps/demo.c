/*
 * demo.c - a small window used to try out the window manager and the
 * widget helpers: a text box, a button that clears it, and a list you can
 * pick from.
 */
#include "apps/demo.h"
#include "gui/wm.h"
#include "gui/widget.h"
#include "mm/kmalloc.h"
#include "lib/string.h"

#define DEMO_WIDTH      360
#define DEMO_HEIGHT     230
#define MARGIN          10
#define TEXT_MAX        64
#define LINE_MAX        48

static const char* const fruits[] = { "Apple", "Banana", "Cherry", "Date", "Elderberry" };
#define FRUIT_COUNT ((int)(sizeof(fruits) / sizeof(fruits[0])))

static const rect_t textbox_rect = { MARGIN, 30, 250, 24 };
static const rect_t clear_rect   = { 270, 30, 80, 24 };
static const rect_t list_rect    = { MARGIN, 90, 200, 5 * WIDGET_LIST_ROW_HEIGHT };

typedef struct {
    char text[TEXT_MAX];
    textbox_t box;
    int selected;
} demo_state;

static void demo_paint(window_t* win) {
    demo_state* state = win->data;
    char line[LINE_MAX];

    widget_label(&win->surface, MARGIN, 8, "Type something:");
    widget_textbox(&win->surface, textbox_rect, &state->box, true);
    widget_button(&win->surface, clear_rect, "Clear");
    widget_label(&win->surface, MARGIN, 68, "Pick a fruit:");
    widget_list(&win->surface, list_rect, fruits, FRUIT_COUNT, 0, state->selected);

    strcpy(line, "Picked: ");
    strcat(line, state->selected >= 0 ? fruits[state->selected] : "nothing");
    widget_label(&win->surface, 220, 90, line);
}

static void demo_key(window_t* win, char key) {
    demo_state* state = win->data;
    if (widget_textbox_key(&state->box, key)) {
        window_invalidate(win);
    }
}

static void demo_mouse(window_t* win, const event_t* e) {
    demo_state* state = win->data;
    if (e->type != EVENT_MOUSE_DOWN) {
        return;
    }
    if (rect_contains(clear_rect, e->x, e->y)) {
        state->text[0] = '\0';
        window_invalidate(win);
        return;
    }
    int index = widget_list_hit(list_rect, FRUIT_COUNT, 0, e->x, e->y);
    if (index >= 0) {
        state->selected = index;
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
    window_t* win = window_create("Widget demo", DEMO_WIDTH, DEMO_HEIGHT);
    if (!win) {
        kfree(state);
        return;
    }
    memset(state, 0, sizeof(demo_state));
    state->box.text = state->text;
    state->box.capacity = TEXT_MAX;
    state->selected = -1;

    win->data = state;
    win->on_paint = demo_paint;
    win->on_key = demo_key;
    win->on_mouse = demo_mouse;
    win->on_close = demo_close;
}
