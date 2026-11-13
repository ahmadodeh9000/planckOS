/*
 * progwin.c - the table of windows that belong to the running program.
 *
 * Each slot links a kernel window_t to the id the program knows it by,
 * and holds the addresses of the program's callbacks (paint, key, mouse,
 * timer). The window manager calls the progwin_* handlers below, which
 * forward each event to the program's function through program_callback.
 *
 * Everything that comes from the program is checked: window ids, callback
 * addresses and string pointers must point into the program area. If any
 * callback crashes, the program is marked dead, all its windows are closed
 * and none of its code is called again.
 */
#include "exec/program.h"
#include "exec/elf.h"
#include "arch/syscall.h"
#include "gfx/surface.h"
#include "gfx/font.h"
#include "exec/progwin.h"
#include "gui/wm.h"
#include "drivers/serial.h"
#include "lib/string.h"

#define PROGWIN_MAX_WIDTH       800     /* largest window a program may open */
#define PROGWIN_MAX_HEIGHT      600
#define PROGWIN_TEXT_MAX        256     /* longest title or text a program may pass */

typedef struct {
    window_t* win;          /* NULL = this slot is free */
    uint32_t on_paint;      /* program function addresses, 0 = not set */
    uint32_t on_key;
    uint32_t on_mouse;
    uint32_t on_timer;
} progwin_t;

static progwin_t slots[PROGWIN_MAX];
static progwin_t* painting;             /* the window inside its paint callback, or NULL */
static char owner[PROGWIN_NAME_MAX];    /* name of the program that owns the windows */
static bool program_dead;               /* it crashed: never call into it again */

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
    if (!fn || program_dead) {
        return;
    }
    program_result_t r = program_callback(fn, progwin_id(pw), a1, a2);
    if (r.crashed) {
        serial_printf("progwin: %s callback of window %u crashed\n",
                      what, progwin_id(pw));
        progwin_program_crashed();
    }
}

/*
 * True if the program's string 's' starts inside the program area and its
 * '\0' comes within max_length characters, also inside the area. Each byte
 * is checked before it is read, so the kernel never reads outside the
 * program area on the program's behalf, even for a string with no end.
 */
static bool progwin_string_ok(const char* s, uint32_t max_length) {
    uint32_t address = (uint32_t)s;

    for (uint32_t i = 0; i <= max_length; i++) {
        if (!elf_in_program_area(address + i)) {
            break;
        }
        if (s[i] == '\0') {
            return true;
        }
    }
    serial_printf("progwin: bad string pointer %x from the program\n", address);
    return false;
}

/* ---- lifetime ----------------------------------------------------- */

void progwin_set_owner(const char* name) {
    uint32_t length = strlen(name);
    if (length >= PROGWIN_NAME_MAX) {
        length = PROGWIN_NAME_MAX - 1;
    }
    memcpy(owner, name, length);
    owner[length] = '\0';
    program_dead = false;       /* a fresh program has been loaded */
}

const char* progwin_owner(void) {
    for (int i = 0; i < PROGWIN_MAX; i++) {
        if (slots[i].win) {
            return owner;
        }
    }
    return NULL;
}

/* Requests (not immediate closes): this may run inside a paint or timer
   callback, where the window manager is still using the windows. */
void progwin_program_crashed(void) {
    int open = 0;

    program_dead = true;
    for (int i = 0; i < PROGWIN_MAX; i++) {
        if (slots[i].win) {
            window_request_close(slots[i].win);
            open++;
        }
    }
    serial_printf("progwin: %s crashed, closing its %d window(s)\n", owner, open);
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
    if (!progwin_string_ok(title, PROGWIN_TEXT_MAX)) {
        return 0;
    }
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
    if (pw && progwin_string_ok(text, PROGWIN_TEXT_MAX)) {
        font_draw_text(&pw->win->surface, x, y, text, color);
    }
}

void progwin_draw_line(uint32_t id, int x0, int y0, int x1, int y1, uint32_t color) {
    progwin_t* pw = progwin_drawing(id);
    if (pw) {
        surface_draw_line(&pw->win->surface, x0, y0, x1, y1, color);
    }
}

void progwin_invalidate(uint32_t id) {
    progwin_t* pw = progwin_get(id);
    if (pw) {
        window_invalidate(pw->win);
    }
}
void progwin_close(uint32_t id) {
    progwin_t* pw = progwin_get(id);
    if (pw) {
        window_request_close(pw->win);
    }
}
