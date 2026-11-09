/*
 * notepad.c - the Notepad app.
 *
 * A file name box with a Save button on top and the text below. Typing
 * adds to the end of the text (Enter starts a new line, backspace deletes
 * the last character); long lines wrap. When the text is taller than the
 * window, the last lines are shown so the end is always visible. Click the
 * name box or the text to choose where typing goes.
 */
#include "apps/notepad.h"
#include "gui/wm.h"
#include "gui/widget.h"
#include "gui/theme.h"
#include "gfx/font.h"
#include "fs/fat12.h"
#include "mm/kmalloc.h"
#include "lib/string.h"

#define NOTEPAD_WIDTH   560
#define NOTEPAD_HEIGHT  420
#define NOTEPAD_MAX     (8 * 1024)      /* largest file we edit */
#define STATUS_MAX      48
#define TITLE_PREFIX    "Notepad - "
#define TEXT_PADDING    4
#define PRINTABLE_FIRST ' '
#define PRINTABLE_LAST  '~'

static const rect_t name_rect = { 56, 8, 140, 24 };
static const rect_t save_rect = { 204, 8, 70, 24 };
static const rect_t text_rect = { 8, 40, NOTEPAD_WIDTH - 16, NOTEPAD_HEIGHT - 48 };

typedef enum { FOCUS_TEXT, FOCUS_NAME } notepad_focus;

typedef struct {
    char name[FAT12_NAME_MAX];
    textbox_t name_box;
    char text[NOTEPAD_MAX + 2];         /* +1 to detect too-long files, +1 for '\0' */
    uint32_t length;
    notepad_focus focus;
    char status[STATUS_MAX];
} notepad_state;

/* ---- drawing the text --------------------------------------------- */

/*
 * Walk the text the way it is laid out on screen (wrapping at 'cols') and
 * call 'draw' for every character on rows from 'first_row' on. Returns the
 * total number of rows, so the caller can work out what to scroll to.
 */
static int text_layout(const notepad_state* n, int cols, int first_row, surface_t* s) {
    int row = 0;
    int col = 0;

    for (uint32_t i = 0; i < n->length; i++) {
        char c = n->text[i];
        if (c == '\n' || col == cols) {
            row++;
            col = 0;
        }
        if (c == '\n') {
            continue;
        }
        if (s && row >= first_row) {
            font_draw_char(s, text_rect.x + TEXT_PADDING + col * FONT_WIDTH,
                           text_rect.y + TEXT_PADDING + (row - first_row) * FONT_HEIGHT, c, THEME_TEXT);
        }
        col++;
    }
    return row + 1;
}

static void notepad_draw_text(window_t* win, notepad_state* n) {
    int cols = (text_rect.w - 2 * TEXT_PADDING) / FONT_WIDTH;
    int rows = (text_rect.h - 2 * TEXT_PADDING) / FONT_HEIGHT;
    int total = text_layout(n, cols, 0, NULL);
    int first = total > rows ? total - rows : 0;
    uint32_t border = n->focus == FOCUS_TEXT ? THEME_TITLE_ACTIVE : THEME_BUTTON_BORDER;

    surface_fill_rect(&win->surface, text_rect.x, text_rect.y, text_rect.w, text_rect.h, THEME_FIELD);
    surface_draw_rect(&win->surface, text_rect.x, text_rect.y, text_rect.w, text_rect.h, border);
    text_layout(n, cols, first, &win->surface);
}

static void notepad_paint(window_t* win) {
    notepad_state* n = win->data;

    widget_label(&win->surface, 8, 12, "File:");
    widget_textbox(&win->surface, name_rect, &n->name_box, n->focus == FOCUS_NAME);
    widget_button(&win->surface, save_rect, "Save");
    widget_label(&win->surface, save_rect.x + save_rect.w + 12, 12, n->status);
    notepad_draw_text(win, n);
}

/* ---- editing and saving ------------------------------------------- */

static void notepad_update_title(window_t* win, const notepad_state* n) {
    char title[WINDOW_TITLE_MAX];
    strcpy(title, TITLE_PREFIX);
    strcat(title, n->name[0] ? n->name : "untitled");
    window_set_title(win, title);
}

static void notepad_save(window_t* win, notepad_state* n) {
    if (!n->name[0]) {
        strcpy(n->status, "Type a file name first");
        n->focus = FOCUS_NAME;
        return;
    }
    fat12_status status = fat12_write(n->name, (const uint8_t*)n->text, n->length);
    if (status == FAT12_OK) {
        strcpy(n->status, "Saved");
        notepad_update_title(win, n);
    } else {
        strcpy(n->status, "Not saved: ");
        strcat(n->status, fat12_status_message(status));
    }
}

static void notepad_text_key(notepad_state* n, char key) {
    if (key == '\b') {
        if (n->length > 0) {
            n->length--;
        }
    } else if (key == '\n' || (key >= PRINTABLE_FIRST && key <= PRINTABLE_LAST)) {
        if (n->length < NOTEPAD_MAX) {
            n->text[n->length++] = key;
        }
    }
}

static void notepad_key(window_t* win, char key) {
    notepad_state* n = win->data;

    if (n->focus == FOCUS_NAME) {
        if (key == '\n') {
            notepad_save(win, n);
        } else {
            widget_textbox_key(&n->name_box, key);
        }
    } else {
        notepad_text_key(n, key);
    }
    window_invalidate(win);
}

static void notepad_mouse(window_t* win, const event_t* e) {
    notepad_state* n = win->data;
    if (e->type != EVENT_MOUSE_DOWN) {
        return;
    }
    if (rect_contains(name_rect, e->x, e->y)) {
        n->focus = FOCUS_NAME;
    } else if (rect_contains(text_rect, e->x, e->y)) {
        n->focus = FOCUS_TEXT;
    } else if (rect_contains(save_rect, e->x, e->y)) {
        notepad_save(win, n);
    }
    window_invalidate(win);
}

static void notepad_close(window_t* win) {
    kfree(win->data);
}

/* ---- opening ------------------------------------------------------ */

static window_t* notepad_create(void) {
    notepad_state* n = kmalloc(sizeof(notepad_state));
    window_t* win = window_create("Notepad", NOTEPAD_WIDTH, NOTEPAD_HEIGHT);
    if (!n || !win) {
        kfree(n);
        if (win) {
            window_close(win);
        }
        return NULL;
    }
    memset(n, 0, sizeof(notepad_state));
    n->name_box.text = n->name;
    n->name_box.capacity = FAT12_NAME_MAX;

    win->data = n;
    win->on_paint = notepad_paint;
    win->on_key = notepad_key;
    win->on_mouse = notepad_mouse;
    win->on_close = notepad_close;
    notepad_update_title(win, n);
    return win;
}

void notepad_open(void) {
    notepad_create();
}

void notepad_open_file(const char* name) {
    window_t* win = notepad_create();
    if (!win) {
        return;
    }
    notepad_state* n = win->data;
    uint32_t name_len = strlen(name);
    if (name_len >= FAT12_NAME_MAX) {
        name_len = FAT12_NAME_MAX - 1;
    }
    memcpy(n->name, name, name_len);
    n->name[name_len] = '\0';
    notepad_update_title(win, n);

    fat12_status status = fat12_read(name, (uint8_t*)n->text, NOTEPAD_MAX + 1, &n->length);
    if (status != FAT12_OK) {
        strcpy(n->status, fat12_status_message(status));
    } else if (n->length > NOTEPAD_MAX) {
        n->length = NOTEPAD_MAX;
        strcpy(n->status, "Only the first 8 KB loaded");
    }
}
