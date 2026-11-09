/*
 * wm.c - window manager: z-order, focus, frames, dragging, closing.
 *
 * Windows live in an array ordered bottom to top; the last one is on top
 * and has keyboard focus. Each window has its own content buffer, so an
 * app only repaints when its content changes (window_invalidate), not
 * every time another window moves over it. wm_draw() then just copies the
 * buffers onto the screen in z-order.
 *
 * Frame layout (B = border width, T = title bar height):
 *
 *   +--------------------------------+  <- (x, y)
 *   | title bar (T high)        [X]  |
 *   +--------------------------------+
 *   | content: win->surface          |
 *   +--------------------------------+
 */
#include "gui/wm.h"
#include "gui/theme.h"
#include "gfx/font.h"
#include "mm/kmalloc.h"
#include "lib/string.h"

#define WM_MAX_WINDOWS      16
#define BYTES_PER_PIXEL     4
#define CASCADE_START_X     60
#define CASCADE_START_Y     40
#define CASCADE_STEP        30
#define CASCADE_SLOTS       8
#define CLOSE_MARGIN        3       /* gap between close button and frame */
#define TITLE_TEXT_X        6
#define MIN_VISIBLE         40      /* pixels of a window that stay on screen */

#define B THEME_BORDER_WIDTH
#define T THEME_TITLE_HEIGHT

static window_t* windows[WM_MAX_WINDOWS];   /* bottom to top */
static int window_count;
static int desktop_width;
static int desktop_height;
static int windows_opened;                  /* for cascading new windows */

static window_t* drag_window;               /* window being moved, or NULL */
static int drag_offset_x;
static int drag_offset_y;
static window_t* mouse_owner;               /* gets moves/release after a press in its content */

/* ---- geometry ----------------------------------------------------- */

rect_t window_frame(const window_t* win) {
    rect_t r = { win->x, win->y, win->surface.width + 2 * B, win->surface.height + T + 2 * B };
    return r;
}

static rect_t window_title_bar(const window_t* win) {
    rect_t r = { win->x + B, win->y + B, win->surface.width, T };
    return r;
}

static rect_t window_close_button(const window_t* win) {
    rect_t title = window_title_bar(win);
    rect_t r = { title.x + title.w - CLOSE_MARGIN - THEME_CLOSE_SIZE,
                 title.y + (T - THEME_CLOSE_SIZE) / 2, THEME_CLOSE_SIZE, THEME_CLOSE_SIZE };
    return r;
}

static rect_t window_content(const window_t* win) {
    rect_t r = { win->x + B, win->y + B + T, win->surface.width, win->surface.height };
    return r;
}

static int clamp(int value, int low, int high) {
    return value < low ? low : (value > high ? high : value);
}

/* ---- the window list ---------------------------------------------- */

void wm_init(int width, int height) {
    desktop_width = width;
    desktop_height = height;
}

static int window_index(const window_t* win) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i] == win) {
            return i;
        }
    }
    return -1;
}

static void window_remove(int index) {
    for (int i = index; i < window_count - 1; i++) {
        windows[i] = windows[i + 1];
    }
    window_count--;
}

void window_raise(window_t* win) {
    int index = window_index(win);
    if (index < 0) {
        return;
    }
    window_remove(index);
    windows[window_count++] = win;
}

window_t* wm_focused(void) {
    return window_count ? windows[window_count - 1] : NULL;
}

int wm_window_count(void) {
    return window_count;
}

window_t* wm_window_at(int index) {
    return (index >= 0 && index < window_count) ? windows[index] : NULL;
}

/* ---- creating and closing ----------------------------------------- */

static void copy_title(char* dest, const char* src) {
    int i = 0;
    for (; src[i] && i < WINDOW_TITLE_MAX - 1; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

window_t* window_create(const char* title, int width, int height) {
    if (window_count == WM_MAX_WINDOWS) {
        return NULL;
    }
    window_t* win = kmalloc(sizeof(window_t));
    uint32_t* pixels = kmalloc(width * height * BYTES_PER_PIXEL);
    if (!win || !pixels) {
        kfree(win);
        kfree(pixels);
        return NULL;
    }

    memset(win, 0, sizeof(window_t));
    copy_title(win->title, title);
    win->surface.pixels = pixels;
    win->surface.width = width;
    win->surface.height = height;
    win->dirty = true;
    win->id = windows_opened;

    int slot = windows_opened++ % CASCADE_SLOTS;
    win->x = CASCADE_START_X + slot * CASCADE_STEP;
    win->y = CASCADE_START_Y + slot * CASCADE_STEP;

    windows[window_count++] = win;
    return win;
}

void window_close(window_t* win) {
    int index = window_index(win);
    if (index < 0) {
        return;
    }
    if (win->on_close) {
        win->on_close(win);
    }
    window_remove(index);
    if (drag_window == win) {
        drag_window = NULL;
    }
    if (mouse_owner == win) {
        mouse_owner = NULL;
    }
    kfree(win->surface.pixels);
    kfree(win);
}

void window_invalidate(window_t* win) {
    win->dirty = true;
}

/* ---- input -------------------------------------------------------- */

/* Topmost window whose frame contains (x, y), or NULL. */
static window_t* window_at_point(int x, int y) {
    for (int i = window_count - 1; i >= 0; i--) {
        if (rect_contains(window_frame(windows[i]), x, y)) {
            return windows[i];
        }
    }
    return NULL;
}

/* Pass a mouse event to the app with coordinates relative to its content. */
static void window_send_mouse(window_t* win, const event_t* e) {
    if (!win->on_mouse) {
        return;
    }
    rect_t content = window_content(win);
    event_t local = *e;
    local.x -= content.x;
    local.y -= content.y;
    win->on_mouse(win, &local);
}

static bool wm_mouse_down(const event_t* e) {
    window_t* win = window_at_point(e->x, e->y);
    if (!win) {
        return false;
    }
    window_raise(win);

    if (e->button != MOUSE_BUTTON_LEFT) {
        return true;
    }
    if (rect_contains(window_close_button(win), e->x, e->y)) {
        window_close(win);
    } else if (rect_contains(window_title_bar(win), e->x, e->y)) {
        drag_window = win;
        drag_offset_x = e->x - win->x;
        drag_offset_y = e->y - win->y;
    } else if (rect_contains(window_content(win), e->x, e->y)) {
        mouse_owner = win;
        window_send_mouse(win, e);
    }
    return true;
}

/* Move the dragged window, keeping its title bar reachable. */
static void wm_drag_to(int x, int y) {
    rect_t frame = window_frame(drag_window);
    drag_window->x = clamp(x - drag_offset_x, MIN_VISIBLE - frame.w, desktop_width - MIN_VISIBLE);
    drag_window->y = clamp(y - drag_offset_y, 0, desktop_height - T);
}

static bool wm_mouse_move(const event_t* e) {
    if (drag_window) {
        wm_drag_to(e->x, e->y);
    } else if (mouse_owner) {
        window_send_mouse(mouse_owner, e);
    }
    return true;    /* the cursor moved, so the screen changes anyway */
}

static bool wm_mouse_up(const event_t* e) {
    if (e->button == MOUSE_BUTTON_LEFT) {
        drag_window = NULL;
        if (mouse_owner) {
            window_t* owner = mouse_owner;
            mouse_owner = NULL;
            window_send_mouse(owner, e);
        }
    }
    return true;
}

bool wm_handle_event(const event_t* e) {
    switch (e->type) {
        case EVENT_MOUSE_DOWN:
            return wm_mouse_down(e);
        case EVENT_MOUSE_MOVE:
            return wm_mouse_move(e);
        case EVENT_MOUSE_UP:
            return wm_mouse_up(e);
        case EVENT_KEY: {
            window_t* focused = wm_focused();
            if (focused && focused->on_key) {
                focused->on_key(focused, e->key);
                return true;
            }
            return false;
        }
        default:
            return false;
    }
}

/* ---- drawing ------------------------------------------------------ */

static void window_draw_frame(surface_t* screen, const window_t* win, bool focused) {
    rect_t frame = window_frame(win);
    rect_t title = window_title_bar(win);
    rect_t close = window_close_button(win);

    surface_draw_rect(screen, frame.x, frame.y, frame.w, frame.h, THEME_BORDER);
    surface_fill_rect(screen, title.x, title.y, title.w, title.h,
                      focused ? THEME_TITLE_ACTIVE : THEME_TITLE_INACTIVE);
    font_draw_text(screen, title.x + TITLE_TEXT_X, title.y + (T - FONT_HEIGHT) / 2,
                   win->title, THEME_TITLE_TEXT);

    surface_fill_rect(screen, close.x, close.y, close.w, close.h, THEME_CLOSE_BUTTON);
    font_draw_char(screen, close.x + (close.w - FONT_WIDTH) / 2,
                   close.y + (close.h - FONT_HEIGHT) / 2, 'x', THEME_TITLE_TEXT);
}

void wm_draw(surface_t* screen) {
    for (int i = 0; i < window_count; i++) {
        window_t* win = windows[i];

        if (win->dirty) {
            surface_fill_rect(&win->surface, 0, 0, win->surface.width, win->surface.height,
                              THEME_WINDOW_BG);
            if (win->on_paint) {
                win->on_paint(win);
            }
            win->dirty = false;
        }

        window_draw_frame(screen, win, i == window_count - 1);
        rect_t content = window_content(win);
        surface_blit(screen, content.x, content.y, &win->surface);
    }
}
