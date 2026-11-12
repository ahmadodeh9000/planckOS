/*
 * notepad.c - the Notepad app: a small text editor for FAT12 files.
 *
 * A file name box with a Save button on top, the text below with a
 * line-number gutter on the left. The text is one flat buffer of
 * characters with '\n' between lines; the cursor is an index into it.
 * Lines and columns are worked out from the buffer when needed (files are
 * at most 8 KB, so scanning is cheap), which keeps the editing code to
 * simple inserts and deletes.
 *
 * Keys: arrows, Home/End, Backspace/Delete, Enter, Tab (4 spaces). The
 * view scrolls to keep the cursor visible. Lines don't wrap.
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
#define POSITION_MAX    24              /* "Ln 123, Col 45" */
#define NUMBER_MAX      12
#define TITLE_PREFIX    "Notepad - "
#define TEXT_PADDING    4
#define GUTTER_DIGITS   4
#define GUTTER_WIDTH    ((GUTTER_DIGITS + 1) * FONT_WIDTH)
#define TAB_WIDTH       4
#define CURSOR_WIDTH    2
#define PRINTABLE_FIRST ' '
#define PRINTABLE_LAST  '~'

static const rect_t name_rect = { 56, 8, 140, 24 };
static const rect_t save_rect = { 204, 8, 70, 24 };
static const rect_t text_rect = { 8, 40, NOTEPAD_WIDTH - 16, NOTEPAD_HEIGHT - 48 };

/* Where the characters go: right of the gutter, inside the padding. */
#define TEXT_X      (text_rect.x + GUTTER_WIDTH + TEXT_PADDING)
#define TEXT_Y      (text_rect.y + TEXT_PADDING)
#define TEXT_COLS   ((text_rect.w - GUTTER_WIDTH - 2 * TEXT_PADDING) / FONT_WIDTH)
#define TEXT_ROWS   ((text_rect.h - 2 * TEXT_PADDING) / FONT_HEIGHT)

typedef enum { FOCUS_TEXT, FOCUS_NAME } notepad_focus;

typedef struct {
    char name[FAT12_NAME_MAX];
    textbox_t name_box;
    char text[NOTEPAD_MAX + 1];         /* +1 to notice files that are too long */
    uint32_t length;
    uint32_t cursor;                    /* index in text where typing goes */
    int first_line;                     /* scroll position: top visible line */
    int first_col;                      /* scroll position: left visible column */
    notepad_focus focus;
    char status[STATUS_MAX];
} notepad_state;

/* ---- lines and columns -------------------------------------------- */

static uint32_t line_start(const notepad_state* n, uint32_t index) {
    while (index > 0 && n->text[index - 1] != '\n') {
        index--;
    }
    return index;
}

static uint32_t line_end(const notepad_state* n, uint32_t index) {
    while (index < n->length && n->text[index] != '\n') {
        index++;
    }
    return index;
}

/* Line and column (both from 0) of the cursor. */
static void cursor_position(const notepad_state* n, int* line, int* col) {
    uint32_t start = line_start(n, n->cursor);

    *line = 0;
    for (uint32_t i = 0; i < start; i++) {
        if (n->text[i] == '\n') {
            (*line)++;
        }
    }
    *col = n->cursor - start;
}

/* Index of (line, col), clamped to the last line and to the line's end. */
static uint32_t index_at(const notepad_state* n, int line, int col) {
    uint32_t index = 0;

    while (line > 0 && index < n->length) {
        if (n->text[index++] == '\n') {
            line--;
        }
    }
    uint32_t end = line_end(n, index);
    return (index + col < end) ? index + col : end;
}

/* ---- editing ------------------------------------------------------ */

static void text_insert(notepad_state* n, const char* chars, uint32_t count) {
    if (n->length + count > NOTEPAD_MAX) {
        strcpy(n->status, "File is full (8 KB)");
        return;
    }
    memmove(n->text + n->cursor + count, n->text + n->cursor, n->length - n->cursor);
    memcpy(n->text + n->cursor, chars, count);
    n->length += count;
    n->cursor += count;
}

/* Delete the character at 'index'. */
static void text_delete(notepad_state* n, uint32_t index) {
    memmove(n->text + index, n->text + index + 1, n->length - index - 1);
    n->length--;
}

static void cursor_move_line(notepad_state* n, int delta) {
    int line, col;
    cursor_position(n, &line, &col);
    if (line + delta >= 0) {
        n->cursor = index_at(n, line + delta, col);
    }
}

/* Keys that move the cursor. Returns false if 'key' isn't one of them. */
static bool notepad_move_key(notepad_state* n, char key) {
    switch (key) {
        case KEY_LEFT:
            if (n->cursor > 0) {
                n->cursor--;
            }
            return true;
        case KEY_RIGHT:
            if (n->cursor < n->length) {
                n->cursor++;
            }
            return true;
        case KEY_UP:
            cursor_move_line(n, -1);
            return true;
        case KEY_DOWN:
            cursor_move_line(n, 1);
            return true;
        case KEY_HOME:
            n->cursor = line_start(n, n->cursor);
            return true;
        case KEY_END:
            n->cursor = line_end(n, n->cursor);
            return true;
        default:
            return false;
    }
}

static void notepad_edit_key(notepad_state* n, char key) {
    static const char tab_spaces[TAB_WIDTH] = { ' ', ' ', ' ', ' ' };

    if (key == '\b' && n->cursor > 0) {
        n->cursor--;
        text_delete(n, n->cursor);
    } else if (key == KEY_DELETE && n->cursor < n->length) {
        text_delete(n, n->cursor);
    } else if (key == '\t') {
        text_insert(n, tab_spaces, TAB_WIDTH);
    } else if (key == '\n' || (key >= PRINTABLE_FIRST && key <= PRINTABLE_LAST)) {
        text_insert(n, &key, 1);
    }
}

/* Scroll just enough that the cursor is inside the visible area. */
static void scroll_to_cursor(notepad_state* n) {
    int line, col;
    cursor_position(n, &line, &col);

    if (line < n->first_line) {
        n->first_line = line;
    } else if (line >= n->first_line + TEXT_ROWS) {
        n->first_line = line - TEXT_ROWS + 1;
    }
    /* If the cursor fits without scrolling sideways, don't: otherwise a
       short line would stay shifted after leaving a long one. */
    if (col < TEXT_COLS) {
        n->first_col = 0;
    } else if (col < n->first_col) {
        n->first_col = col;
    } else if (col >= n->first_col + TEXT_COLS) {
        n->first_col = col - TEXT_COLS + 1;
    }
}

/* ---- drawing ------------------------------------------------------ */

/* Draw the characters of every visible line, skipping scrolled-off columns. */
static void draw_text(surface_t* s, const notepad_state* n) {
    int line = 0;
    int col = 0;

    for (uint32_t i = 0; i < n->length && line < n->first_line + TEXT_ROWS; i++) {
        if (n->text[i] == '\n') {
            line++;
            col = 0;
            continue;
        }
        int row = line - n->first_line;
        int column = col - n->first_col;
        if (row >= 0 && column >= 0 && column < TEXT_COLS) {
            font_draw_char(s, TEXT_X + column * FONT_WIDTH, TEXT_Y + row * FONT_HEIGHT,
                           n->text[i], THEME_TEXT);
        }
        col++;
    }
}

/* Line numbers (from 1), right-aligned in the gutter. */
static void draw_gutter(surface_t* s, const notepad_state* n) {
    int total_lines = 1;
    char number[NUMBER_MAX];

    for (uint32_t i = 0; i < n->length; i++) {
        if (n->text[i] == '\n') {
            total_lines++;
        }
    }
    surface_fill_rect(s, text_rect.x, text_rect.y, GUTTER_WIDTH, text_rect.h, THEME_BUTTON);
    for (int row = 0; row < TEXT_ROWS && n->first_line + row < total_lines; row++) {
        utoa(n->first_line + row + 1, number, 10);
        int x = text_rect.x + GUTTER_WIDTH - (strlen(number) + 1) * FONT_WIDTH;
        font_draw_text(s, x, TEXT_Y + row * FONT_HEIGHT, number, THEME_BUTTON_BORDER);
    }
}

static void draw_cursor(surface_t* s, const notepad_state* n) {
    int line, col;
    cursor_position(n, &line, &col);
    surface_fill_rect(s, TEXT_X + (col - n->first_col) * FONT_WIDTH,
                      TEXT_Y + (line - n->first_line) * FONT_HEIGHT,
                      CURSOR_WIDTH, FONT_HEIGHT, THEME_TEXT);
}

/* "Ln 3, Col 7" at the right end of the toolbar (both from 1). */
static void draw_position(surface_t* s, const notepad_state* n) {
    char text[POSITION_MAX];
    char number[NUMBER_MAX];
    int line, col;

    cursor_position(n, &line, &col);
    strcpy(text, "Ln ");
    strcat(text, utoa(line + 1, number, 10));
    strcat(text, ", Col ");
    strcat(text, utoa(col + 1, number, 10));
    widget_label(s, text_rect.x + text_rect.w - strlen(text) * FONT_WIDTH, 12, text);
}

static void notepad_paint(window_t* win) {
    notepad_state* n = win->data;
    surface_t* s = &win->surface;
    uint32_t border = n->focus == FOCUS_TEXT ? THEME_TITLE_ACTIVE : THEME_BUTTON_BORDER;

    widget_label(s, 8, 12, "File:");
    widget_textbox(s, name_rect, &n->name_box, n->focus == FOCUS_NAME);
    widget_button(s, save_rect, "Save");
    widget_label(s, save_rect.x + save_rect.w + 12, 12, n->status);
    draw_position(s, n);

    surface_fill_rect(s, text_rect.x, text_rect.y, text_rect.w, text_rect.h, THEME_FIELD);
    draw_gutter(s, n);
    draw_text(s, n);
    if (n->focus == FOCUS_TEXT) {
        draw_cursor(s, n);
    }
    surface_draw_rect(s, text_rect.x, text_rect.y, text_rect.w, text_rect.h, border);
}

/* ---- saving and window callbacks ---------------------------------- */

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

static void notepad_key(window_t* win, char key) {
    notepad_state* n = win->data;

    if (n->focus == FOCUS_NAME) {
        if (key == '\n') {
            notepad_save(win, n);
        } else {
            widget_textbox_key(&n->name_box, key);
        }
    } else {
        n->status[0] = '\0';
        if (!notepad_move_key(n, key)) {
            notepad_edit_key(n, key);
        }
        scroll_to_cursor(n);
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
