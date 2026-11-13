/*
 * progwin.c - the table of windows that belong to the running program.
 *
 * Each slot links a kernel window_t to the id the program knows it by,
 * and holds the addresses of the program's callbacks (paint, key, mouse,
 * timer). The window manager calls the progwin_* handlers below, which
 * forward each event to the program's function through program_callback.
 */
#include "exec/program.h"
#include "exec/elf.h"
#include "arch/syscall.h"
#include "gfx/surface.h"
#include "gfx/font.h"
#include "exec/progwin.h"
#include "gui/wm.h"
#include "drivers/serial.h"

#define PROGWIN_MAX_WIDTH       800     /* largest window a program may open */
#define PROGWIN_MAX_HEIGHT      600

typedef struct {
    window_t* win;          /* NULL = this slot is free */
    uint32_t on_paint;      /* program function addresses, 0 = not set */
    uint32_t on_key;
    uint32_t on_mouse;
    uint32_t on_timer;
} progwin_t;

static progwin_t slots[PROGWIN_MAX];
static progwin_t* painting;             /* the window inside its paint callback, or NULL */

/* slots[0] is id 1, slots[1] is id 2, ...; 0 is never a valid id. */
static uint32_t progwin_id(const progwin_t* pw) {
    return (uint32_t)(pw - slots) + 1;
}

/* The slot for 'id', or NULL if the id is invalid or its window is closed. */
static progwin_t* progwin_get(uint32_t id) {
    if (id < 1 || id > PROGWIN_MAX) {
        return NULL;
    }
    progwin_t* pw = &slots[id - 1];
    return pw->win ? pw : NULL;
}

static progwin_t* progwin_free_slot(void) {
    for (int i = 0; i < PROGWIN_MAX; ++i) {
        if (slots[i].win == NULL) {
            return &slots[i];
        }
    }
    return NULL;
}

/* The window manager calls this when the window is closed (by the user's
   close button or anything else), so the slot can be reused. */
static void progwin_closed(window_t* win) {
    progwin_t* pw = win->data;
    *pw = (progwin_t){ 0 };     /* window and callbacks gone */
}

/* Call the program's function 'fn' as fn(id, a1, a2), if it set one. */
static void progwin_call(progwin_t* pw, uint32_t fn, uint32_t a1, uint32_t a2,
                         const char* what) {
    if (!fn) {
        return;
    }
    program_result_t r = program_callback(fn, progwin_id(pw), a1, a2);
    if (r.crashed) {
        serial_printf("progwin: %s callback of window %u crashed\n",
                      what, progwin_id(pw));
    }
}

static void progwin_paint(window_t* win) {
    progwin_t* pw = win->data;
    painting = pw;
    progwin_call(pw, pw->on_paint, 0, 0, "paint");
    painting = NULL;
}

static void progwin_key(window_t* win, char key) {
    progwin_t* pw = win->data;
    progwin_call(pw, pw->on_key, (uint8_t)key, 0, "key");
}

static void progwin_mouse(window_t* win, const event_t* e) {
    progwin_t* pw = win->data;
    if (e->type == EVENT_MOUSE_DOWN) {
        progwin_call(pw, pw->on_mouse, e->x, e->y, "mouse");
    }
}

static void progwin_timer(window_t* win) {
    progwin_t* pw = win->data;
    progwin_call(pw, pw->on_timer, 0, 0, "timer");
}

/* ---- the syscalls ------------------------------------------------- */

uint32_t progwin_create(const char* title, int width, int height) {
    if (width <= 0 || height <= 0 ||
        width > PROGWIN_MAX_WIDTH || height > PROGWIN_MAX_HEIGHT) {
        serial_printf("progwin: bad window size %dx%d\n", width, height);
        return 0;
    }

    progwin_t* pw = progwin_free_slot();
    if (pw == NULL) {
        serial_printf("progwin: program already has %d windows\n", PROGWIN_MAX);
        return 0;
    }

    window_t* win = window_create(title, width, height);
    if (!win) {
        return 0;
    }

    pw->win = win;
    win->data = pw;                 /* so callbacks can find the slot */
    win->on_paint = progwin_paint;
    win->on_close = progwin_closed;
    win->on_key   = progwin_key;
    win->on_mouse = progwin_mouse;
    win->on_timer = progwin_timer;

    return progwin_id(pw);
}

bool progwin_set_callback(uint32_t id, uint32_t event, uint32_t fn) {
    progwin_t* pw = progwin_get(id);
    if (!pw || !elf_in_program_area(fn)) {
        return false;
    }
    switch (event) {
        case WIN_EVENT_PAINT:
            pw->on_paint = fn;
            break;
        case WIN_EVENT_KEY:
            pw->on_key = fn;
            break;
        case WIN_EVENT_MOUSE:
            pw->on_mouse = fn;
            break;
        case WIN_EVENT_TIMER:
            pw->on_timer = fn;
            break;
        default:
            return false;
    }
    window_invalidate(pw->win);     /* paint with the new function */
    return true;
}

/* The slot for 'id', if that window is the one currently painting. */
static progwin_t* progwin_drawing(uint32_t id) {
    progwin_t* pw = progwin_get(id);
    return (pw && pw == painting) ? pw : NULL;
}

void progwin_fill_rect(uint32_t id, int x, int y, int w, int h, uint32_t color) {
    progwin_t* pw = progwin_drawing(id);
    if (pw) {
        surface_fill_rect(&pw->win->surface, x, y, w, h, color);
    }
}

void progwin_draw_text(uint32_t id, int x, int y, const char* text, uint32_t color) {
    progwin_t* pw = progwin_drawing(id);
    if (pw) {
        font_draw_text(&pw->win->surface, x, y, text, color);
    }
}

void progwin_invalidate(uint32_t id) {
    progwin_t* pw = progwin_get(id);
    if (pw) {
        window_invalidate(pw->win);
    }
}