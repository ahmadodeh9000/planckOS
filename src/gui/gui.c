/*
 * gui.c - the GUI main loop and screen composition.
 *
 *   event -> dispatch -> compose -> present -> sleep (hlt)
 *
 * The loop sleeps until an input event arrives, hands every queued event
 * to the taskbar or the window manager, and redraws only if something
 * changed. A frame is composed back to front: desktop, windows (bottom to
 * top), taskbar, mouse cursor.
 * Drawing happens here, in normal kernel context, never inside an
 * interrupt handler.
 */
#include "gui/gui.h"
#include "gui/event.h"
#include "gui/cursor.h"
#include "gui/theme.h"
#include "gui/wm.h"
#include "gui/taskbar.h"
#include "apps/demo.h"
#include "gfx/fb.h"

static int mouse_x;
static int mouse_y;

static bool gui_dispatch(const event_t* e) {
    switch (e->type) {
        case EVENT_TIMER:
            return true;    /* the taskbar clock shows a new second */
        case EVENT_KEY:
            return wm_handle_event(e);
        case EVENT_MOUSE_DOWN:
            mouse_x = e->x;
            mouse_y = e->y;
            return taskbar_mouse_down(e) || wm_handle_event(e);
        default:
            mouse_x = e->x;
            mouse_y = e->y;
            return wm_handle_event(e);
    }
}

static void gui_compose(surface_t* screen) {
    surface_fill_rect(screen, 0, 0, screen->width, screen->height, THEME_DESKTOP);
    wm_draw(screen);
    taskbar_draw(screen);
    cursor_draw(screen, mouse_x, mouse_y);
}

void gui_main_loop(void) {
    surface_t* screen = fb_back_buffer();
    event_t event;

    mouse_x = screen->width / 2;
    mouse_y = screen->height / 2;
    wm_init(screen->width, screen->height - THEME_TASKBAR_HEIGHT);
    taskbar_init(screen->width, screen->height);
    demo_open();
    demo_open();

    gui_compose(screen);
    fb_present();
    for (;;) {
        event_wait(&event);
        bool changed = gui_dispatch(&event);
        while (event_poll(&event)) {
            changed |= gui_dispatch(&event);
        }
        if (changed) {
            gui_compose(screen);
            fb_present();
        }
    }
}
