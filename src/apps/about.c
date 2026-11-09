/*
 * about.c - the About app: OS name, version, kernel, author, uptime and
 * memory use. It repaints once a second so the uptime and memory figures
 * stay current.
 */
#include "apps/about.h"
#include "gui/wm.h"
#include "gui/widget.h"
#include "gui/theme.h"
#include "gfx/font.h"
#include "drivers/timer.h"
#include "mm/pmm.h"
#include "mm/kmalloc.h"
#include "lib/string.h"

#define ABOUT_WIDTH     380
#define ABOUT_HEIGHT    190
#define MARGIN          16
#define VALUE_X         110
#define LINE_SPACING    (FONT_HEIGHT + 6)
#define LINE_MAX        48
#define NUMBER_MAX      12
#define KB              1024
#define MB              (1024 * 1024)

/* "<used> <unit> used, <free> <unit> free" */
static void format_usage(char* out, uint32_t used, uint32_t free, uint32_t unit, const char* unit_name) {
    char number[NUMBER_MAX];

    strcpy(out, utoa(used / unit, number, 10));
    strcat(out, unit_name);
    strcat(out, " used, ");
    strcat(out, utoa(free / unit, number, 10));
    strcat(out, unit_name);
    strcat(out, " free");
}

static void about_row(surface_t* s, int row, const char* name, const char* value) {
    int y = MARGIN + row * LINE_SPACING;
    widget_label(s, MARGIN, y, name);
    widget_label(s, VALUE_X, y, value);
}

static void about_paint(window_t* win) {
    surface_t* s = &win->surface;
    char line[LINE_MAX];
    uint32_t total_pages, used_pages, heap_used, heap_free;

    font_draw_text(s, MARGIN, MARGIN, OS_NAME " " OS_VERSION, THEME_TITLE_ACTIVE);
    about_row(s, 1, "Kernel:", KERNEL_NAME);
    about_row(s, 2, "Author:", "Ahmad Odeh");

    timer_format_uptime(line);
    about_row(s, 3, "Uptime:", line);

    pmm_stats(&total_pages, &used_pages);
    format_usage(line, used_pages * PAGE_SIZE, (total_pages - used_pages) * PAGE_SIZE, MB, " MB");
    about_row(s, 4, "RAM:", line);

    heap_stats(&heap_used, &heap_free);
    format_usage(line, heap_used, heap_free, KB, " KB");
    about_row(s, 5, "Heap:", line);
}

static void about_timer(window_t* win) {
    window_invalidate(win);
}

void about_open(void) {
    window_t* win = window_create("About " OS_NAME, ABOUT_WIDTH, ABOUT_HEIGHT);
    if (!win) {
        return;
    }
    win->on_paint = about_paint;
    win->on_timer = about_timer;
}
