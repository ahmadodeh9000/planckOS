/*
 * taskbar.c - launcher button, window buttons and uptime clock.
 *
 * The launcher button is drawn here, but clicks on it are handled by the
 * launcher menu (menu.c), which pops up above it.
 * Window buttons are listed in the order the windows were opened (not
 * z-order), so they don't jump around when you click between windows. The
 * focused window's button is highlighted; clicking a button raises its
 * window.
 */
#include "gui/taskbar.h"
#include "gui/theme.h"
#include "gui/wm.h"
#include "gui/menu.h"
#include "gfx/font.h"
#include "drivers/timer.h"
#include "lib/string.h"

#define BUTTON_GAP          4
#define BUTTON_MARGIN_Y     3
#define LAUNCHER_WIDTH      96
#define WINDOW_BUTTON_WIDTH 150
#define TEXT_PADDING        8
#define MAX_TASK_BUTTONS    16

static rect_t bar;

static rect_t launcher_button(void) {
    rect_t r = { BUTTON_GAP, bar.y + BUTTON_MARGIN_Y, LAUNCHER_WIDTH,
                 bar.h - 2 * BUTTON_MARGIN_Y };
    return r;
}

void taskbar_init(int screen_width, int screen_height) {
    bar.x = 0;
    bar.y = screen_height - THEME_TASKBAR_HEIGHT;
    bar.w = screen_width;
    bar.h = THEME_TASKBAR_HEIGHT;
    menu_init(launcher_button());
}

static rect_t window_button(int slot) {
    rect_t r = launcher_button();
    r.x += LAUNCHER_WIDTH + BUTTON_GAP + slot * (WINDOW_BUTTON_WIDTH + BUTTON_GAP);
    r.w = WINDOW_BUTTON_WIDTH;
    return r;
}

/* Fill 'out' with the open windows sorted by creation order. */
static int windows_in_open_order(window_t** out) {
    int count = wm_window_count();

    for (int i = 0; i < count && i < MAX_TASK_BUTTONS; i++) {
        window_t* win = wm_window_at(i);
        int j = i;
        while (j > 0 && out[j - 1]->id > win->id) {     /* insertion sort */
            out[j] = out[j - 1];
            j--;
        }
        out[j] = win;
    }
    return count < MAX_TASK_BUTTONS ? count : MAX_TASK_BUTTONS;
}

bool taskbar_mouse_down(const event_t* e) {
    if (!rect_contains(bar, e->x, e->y)) {
        return false;
    }
    window_t* list[MAX_TASK_BUTTONS];
    int count = windows_in_open_order(list);
    for (int i = 0; i < count; i++) {
        if (rect_contains(window_button(i), e->x, e->y)) {
            window_raise(list[i]);
        }
    }
    return true;
}

/* Draw a taskbar button with 'label', cut short to fit. */
static void taskbar_button(surface_t* screen, rect_t r, const char* label, uint32_t color) {
    int max_chars = (r.w - 2 * TEXT_PADDING) / FONT_WIDTH;

    surface_fill_rect(screen, r.x, r.y, r.w, r.h, color);
    for (int i = 0; label[i] && i < max_chars; i++) {
        font_draw_char(screen, r.x + TEXT_PADDING + i * FONT_WIDTH,
                       r.y + (r.h - FONT_HEIGHT) / 2, label[i], THEME_TEXT_LIGHT);
    }
}

void taskbar_draw(surface_t* screen) {
    window_t* list[MAX_TASK_BUTTONS];
    int count = windows_in_open_order(list);
    window_t* focused = wm_focused();
    char clock[TIMER_UPTIME_LEN + 1];

    surface_fill_rect(screen, bar.x, bar.y, bar.w, bar.h, THEME_TASKBAR);
    taskbar_button(screen, launcher_button(), OS_NAME, THEME_TASKBAR_ACTIVE);
    for (int i = 0; i < count; i++) {
        uint32_t color = list[i] == focused ? THEME_TASKBAR_ACTIVE : THEME_TASKBAR_BUTTON;
        taskbar_button(screen, window_button(i), list[i]->title, color);
    }

    timer_format_uptime(clock);
    font_draw_text(screen, bar.w - (TIMER_UPTIME_LEN + 1) * FONT_WIDTH,
                   bar.y + (bar.h - FONT_HEIGHT) / 2, clock, THEME_TEXT_LIGHT);
}
