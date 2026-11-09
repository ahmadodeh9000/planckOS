/*
 * gui.c - the GUI main loop.
 *
 * The loop sleeps until an input event arrives, applies every queued
 * event, and redraws only if something changed. Drawing happens here, in
 * normal kernel context, never inside an interrupt handler.
 *
 * For now the screen is a placeholder that shows the live input state; the
 * window manager replaces it.
 */
#include "gui/gui.h"
#include "gui/event.h"
#include "gfx/fb.h"
#include "gfx/font.h"
#include "lib/string.h"

#define COLOR_DESKTOP   RGB(0x20, 0x4A, 0x87)
#define COLOR_PANEL     RGB(0xEE, 0xEE, 0xEC)
#define COLOR_TITLE     RGB(0x34, 0x65, 0xA4)
#define COLOR_TEXT      RGB(0x00, 0x00, 0x00)
#define COLOR_WHITE     RGB(0xFF, 0xFF, 0xFF)
#define LINE_HEIGHT     FONT_HEIGHT
#define LABEL_MAX       64
#define NUMBER_MAX      12

typedef struct {
    int mouse_x, mouse_y;
    uint8_t buttons;
    char last_key;
} input_state;

static input_state state;

static void gui_apply_event(const event_t* e) {
    switch (e->type) {
        case EVENT_KEY:
            state.last_key = e->key;
            break;
        case EVENT_MOUSE_MOVE:
            state.mouse_x = e->x;
            state.mouse_y = e->y;
            break;
        case EVENT_MOUSE_DOWN:
            state.buttons |= e->button;
            break;
        case EVENT_MOUSE_UP:
            state.buttons &= ~e->button;
            break;
    }
}

/* "Mouse: 512, 384  buttons: L - -" */
static void format_mouse_line(char* out) {
    char number[NUMBER_MAX];

    strcpy(out, "Mouse: ");
    strcat(out, utoa(state.mouse_x, number, 10));
    strcat(out, ", ");
    strcat(out, utoa(state.mouse_y, number, 10));
    strcat(out, "  buttons: ");
    strcat(out, (state.buttons & MOUSE_BUTTON_LEFT) ? "L " : "- ");
    strcat(out, (state.buttons & MOUSE_BUTTON_MIDDLE) ? "M " : "- ");
    strcat(out, (state.buttons & MOUSE_BUTTON_RIGHT) ? "R" : "-");
}

static void gui_draw(surface_t* screen) {
    int w = screen->width;
    int h = screen->height;
    int x = w / 4 + FONT_WIDTH;
    int y = h / 4 + 4;
    char line[LABEL_MAX];

    surface_fill_rect(screen, 0, 0, w, h, COLOR_DESKTOP);
    surface_fill_rect(screen, w / 4, h / 4, w / 2, h / 2, COLOR_PANEL);
    surface_fill_rect(screen, w / 4, h / 4, w / 2, LINE_HEIGHT + 8, COLOR_TITLE);
    font_draw_text(screen, x, y, OS_NAME " " OS_VERSION " - kernel " KERNEL_NAME, COLOR_WHITE);

    format_mouse_line(line);
    font_draw_text(screen, x, y + 2 * LINE_HEIGHT, line, COLOR_TEXT);

    strcpy(line, "Last key: ");
    line[strlen(line) + 1] = '\0';
    line[strlen(line)] = state.last_key ? state.last_key : '-';
    font_draw_text(screen, x, y + 3 * LINE_HEIGHT, line, COLOR_TEXT);
}

void gui_main_loop(void) {
    surface_t* screen = fb_back_buffer();
    event_t event;

    state.mouse_x = screen->width / 2;
    state.mouse_y = screen->height / 2;
    gui_draw(screen);
    fb_present();

    for (;;) {
        event_wait(&event);
        gui_apply_event(&event);
        while (event_poll(&event)) {
            gui_apply_event(&event);
        }
        gui_draw(screen);
        fb_present();
    }
}
