/*
 * wm.h - the window manager.
 *
 * A window is a title bar plus a content area with its own pixel buffer
 * (win->surface). Apps draw their content in on_paint() and react to input
 * in on_key() / on_mouse(). The window manager keeps the windows in
 * z-order, draws their frames, and handles focus, raising, dragging and
 * the close button.
 */
#ifndef wm_h__
#define wm_h__

#include "lib/common.h"
#include "gfx/surface.h"
#include "gui/event.h"

#define WINDOW_TITLE_MAX    32

typedef struct window window_t;

struct window {
    char title[WINDOW_TITLE_MAX];
    int x, y;                   /* top-left of the frame, in screen pixels */
    surface_t surface;          /* content pixels; its size is the content size */
    bool dirty;                 /* content must be repainted (on_paint) */
    void* data;                 /* the app's own state */

    /* Callbacks, all optional (NULL = ignore). */
    void (*on_paint)(window_t* win);                    /* draw into win->surface */
    void (*on_key)(window_t* win, char key);
    void (*on_mouse)(window_t* win, const event_t* e);  /* x, y relative to content */
    void (*on_close)(window_t* win);                    /* free win->data */
};

/* Tell the window manager how big the desktop is (the screen minus the
   taskbar). Windows can't be dragged out of it. */
void wm_init(int desktop_width, int desktop_height);

/* Open a window with a content area of width x height pixels. It starts
   focused, on top, and dirty. Returns NULL if out of memory/slots. */
window_t* window_create(const char* title, int width, int height);

/* Call on_close, free the window and remove it from the screen. */
void window_close(window_t* win);

/* Put the window on top and give it keyboard focus. */
void window_raise(window_t* win);

/* Ask for the content to be repainted on the next frame. */
void window_invalidate(window_t* win);

/* Frame rectangle on screen (title bar + border + content). */
rect_t window_frame(const window_t* win);

/* The focused (top) window, or NULL. */
window_t* wm_focused(void);

/* Windows in z-order, 0 = bottom. Used by the taskbar. */
int wm_window_count(void);
window_t* wm_window_at(int index);

/* Handle a mouse or key event. Returns true if the screen needs to be
   redrawn. */
bool wm_handle_event(const event_t* e);

/* Paint dirty windows and draw all windows onto 'screen', bottom first. */
void wm_draw(surface_t* screen);

#endif
