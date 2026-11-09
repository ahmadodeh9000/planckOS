/*
 * menu.c - the launcher menu.
 *
 * One row per entry in app_list. The row under the pointer is highlighted;
 * clicking it closes the menu and calls the app's open function.
 */
#include "gui/menu.h"
#include "gui/theme.h"
#include "apps/apps.h"
#include "gfx/font.h"

#define MENU_WIDTH      180
#define ROW_HEIGHT      24
#define TEXT_PADDING    10

static bool open;
static rect_t launcher;
static int hovered = -1;

void menu_init(rect_t launcher_button) {
    launcher = launcher_button;
}

static rect_t menu_rect(void) {
    int height = app_count * ROW_HEIGHT;
    rect_t r = { launcher.x, launcher.y - height, MENU_WIDTH, height };
    return r;
}

/* Row under (x, y), or -1. */
static int menu_row_at(int x, int y) {
    rect_t r = menu_rect();
    if (!rect_contains(r, x, y)) {
        return -1;
    }
    return (y - r.y) / ROW_HEIGHT;
}

bool menu_mouse_down(const event_t* e) {
    if (rect_contains(launcher, e->x, e->y)) {
        open = !open;
        hovered = -1;
        return true;
    }
    if (!open) {
        return false;
    }

    int row = menu_row_at(e->x, e->y);
    open = false;
    if (row < 0) {
        return false;
    }
    app_list[row].open();
    return true;
}

void menu_mouse_move(const event_t* e) {
    hovered = open ? menu_row_at(e->x, e->y) : -1;
}

void menu_draw(surface_t* screen) {
    if (!open) {
        return;
    }
    rect_t r = menu_rect();

    surface_fill_rect(screen, r.x, r.y, r.w, r.h, THEME_WINDOW_BG);
    for (int i = 0; i < app_count; i++) {
        int y = r.y + i * ROW_HEIGHT;
        if (i == hovered) {
            surface_fill_rect(screen, r.x, y, r.w, ROW_HEIGHT, THEME_SELECTION);
        }
        font_draw_text(screen, r.x + TEXT_PADDING, y + (ROW_HEIGHT - FONT_HEIGHT) / 2,
                       app_list[i].name, THEME_TEXT);
    }
    surface_draw_rect(screen, r.x, r.y, r.w, r.h, THEME_BORDER);
}
