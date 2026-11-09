/*
 * files.c - the Files app: lists the files on the FAT12 disk.
 *
 * Click a file to select it; double-click (or press Open) to edit it in
 * Notepad; Delete removes the selected file. The list re-reads the disk
 * once a second and only repaints if something changed, so files saved
 * from Notepad or the Terminal show up on their own.
 */
#include "apps/files.h"
#include "apps/notepad.h"
#include "gui/wm.h"
#include "gui/widget.h"
#include "fs/fat12.h"
#include "drivers/timer.h"
#include "mm/kmalloc.h"
#include "lib/string.h"

#define FILES_WIDTH         430
#define FILES_HEIGHT        340
#define FILES_MAX           128
#define LINE_MAX            32
#define STATUS_MAX          48
#define LIST_ROWS           14
#define NAME_COLUMN_CHARS   14      /* size column starts after this */
#define DOUBLE_CLICK_TICKS  (TIMER_HZ / 2)  /* 0.5 s between the two clicks */
#define NUMBER_MAX          12

static const rect_t list_rect   = { 8, 8, 300, LIST_ROWS * WIDGET_LIST_ROW_HEIGHT };
static const rect_t open_rect   = { 320, 8, 100, 26 };
static const rect_t delete_rect = { 320, 42, 100, 26 };
static const rect_t up_rect     = { 320, 228, 100, 26 };
static const rect_t down_rect   = { 320, 262, 100, 26 };
#define STATUS_Y (list_rect.y + list_rect.h + 12)

typedef struct {
    fat12_file_t files[FILES_MAX];
    int count;
    char lines[FILES_MAX][LINE_MAX];    /* "NAME.TXT      123 B" */
    const char* items[FILES_MAX];       /* pointers into 'lines' for widget_list */
    int selected;
    int first;                          /* first visible row */
    int last_click_index;
    uint32_t last_click_tick;
    char status[STATUS_MAX];
} files_state;

/* ---- the file list ------------------------------------------------ */

static void files_format_line(files_state* f, int i) {
    char number[NUMBER_MAX];
    char* line = f->lines[i];
    int len = strlen(f->files[i].name);

    strcpy(line, f->files[i].name);
    while (len < NAME_COLUMN_CHARS) {
        line[len++] = ' ';
    }
    line[len] = '\0';
    strcat(line, utoa(f->files[i].size, number, 10));
    strcat(line, " B");
    f->items[i] = line;
}

/* Re-read the directory. Returns true if the list changed. */
static bool files_reload(files_state* f) {
    /* On the heap: the kernel stack is only a few KB. Zeroed so that bytes
       after each name's '\0' compare equal too. */
    fat12_file_t* fresh = kmalloc(FILES_MAX * sizeof(fat12_file_t));
    if (!fresh) {
        return false;
    }
    memset(fresh, 0, FILES_MAX * sizeof(fat12_file_t));
    int count = fat12_list(fresh, FILES_MAX);

    bool same = count == f->count && memcmp(fresh, f->files, count * sizeof(fat12_file_t)) == 0;
    if (!same) {
        memcpy(f->files, fresh, count * sizeof(fat12_file_t));
        f->count = count;
    }
    kfree(fresh);
    if (same) {
        return false;
    }
    for (int i = 0; i < count; i++) {
        files_format_line(f, i);
    }
    if (f->selected >= count) {
        f->selected = -1;
    }
    if (f->first > 0 && f->first >= count) {
        f->first = 0;
    }
    return true;
}

/* ---- actions ------------------------------------------------------ */

static void files_open_selected(files_state* f) {
    if (f->selected >= 0) {
        notepad_open_file(f->files[f->selected].name);
    }
}

static void files_delete_selected(files_state* f) {
    if (f->selected < 0) {
        strcpy(f->status, "Select a file first");
        return;
    }
    fat12_status status = fat12_delete(f->files[f->selected].name);
    strcpy(f->status, status == FAT12_OK ? "Deleted " : "Not deleted: ");
    strcat(f->status, status == FAT12_OK ? f->files[f->selected].name : fat12_status_message(status));
    f->selected = -1;
    files_reload(f);
}

static void files_scroll(files_state* f, int rows) {
    int max_first = f->count > LIST_ROWS ? f->count - LIST_ROWS : 0;
    f->first += rows;
    if (f->first < 0) {
        f->first = 0;
    }
    if (f->first > max_first) {
        f->first = max_first;
    }
}

/* A click on a list row: select it, or open it if it is a double-click. */
static void files_click_row(files_state* f, int index) {
    uint32_t now = timer_ticks();

    if (index == f->last_click_index && now - f->last_click_tick <= DOUBLE_CLICK_TICKS) {
        files_open_selected(f);
        f->last_click_index = -1;
        return;
    }
    f->selected = index;
    f->last_click_index = index;
    f->last_click_tick = now;
}

/* ---- window callbacks --------------------------------------------- */

static void files_paint(window_t* win) {
    files_state* f = win->data;

    widget_list(&win->surface, list_rect, f->items, f->count, f->first, f->selected);
    widget_button(&win->surface, open_rect, "Open");
    widget_button(&win->surface, delete_rect, "Delete");
    widget_button(&win->surface, up_rect, "Up");
    widget_button(&win->surface, down_rect, "Down");
    widget_label(&win->surface, list_rect.x, STATUS_Y,
                 f->status[0] ? f->status : (f->count ? "Double-click a file to edit it" : "No files"));
}

static void files_mouse(window_t* win, const event_t* e) {
    files_state* f = win->data;
    if (e->type != EVENT_MOUSE_DOWN) {
        return;
    }
    f->status[0] = '\0';

    int index = widget_list_hit(list_rect, f->count, f->first, e->x, e->y);
    if (index >= 0) {
        files_click_row(f, index);
    } else if (rect_contains(open_rect, e->x, e->y)) {
        files_open_selected(f);
    } else if (rect_contains(delete_rect, e->x, e->y)) {
        files_delete_selected(f);
    } else if (rect_contains(up_rect, e->x, e->y)) {
        files_scroll(f, -LIST_ROWS);
    } else if (rect_contains(down_rect, e->x, e->y)) {
        files_scroll(f, LIST_ROWS);
    }
    window_invalidate(win);
}

static void files_timer(window_t* win) {
    if (files_reload(win->data)) {
        window_invalidate(win);
    }
}

static void files_close(window_t* win) {
    kfree(win->data);
}

void files_open(void) {
    files_state* f = kmalloc(sizeof(files_state));
    window_t* win = window_create("Files", FILES_WIDTH, FILES_HEIGHT);
    if (!f || !win) {
        kfree(f);
        if (win) {
            window_close(win);
        }
        return;
    }
    memset(f, 0, sizeof(files_state));
    f->selected = -1;
    f->last_click_index = -1;
    f->count = -1;          /* force the first reload to fill the list */
    files_reload(f);

    win->data = f;
    win->on_paint = files_paint;
    win->on_mouse = files_mouse;
    win->on_timer = files_timer;
    win->on_close = files_close;
}
