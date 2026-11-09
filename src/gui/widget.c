/*
 * widget.c - drawing for labels, buttons, text boxes and lists.
 */
#include "gui/widget.h"
#include "gui/theme.h"
#include "gfx/font.h"
#include "lib/string.h"

#define TEXT_PADDING    4
#define CARET_WIDTH     1
#define PRINTABLE_FIRST ' '
#define PRINTABLE_LAST  '~'

/* y that vertically centers one line of text in a box of height h. */
static int center_text_y(int y, int h) {
    return y + (h - FONT_HEIGHT) / 2;
}

void widget_label(surface_t* s, int x, int y, const char* text) {
    font_draw_text(s, x, y, text, THEME_TEXT);
}

void widget_button(surface_t* s, rect_t r, const char* label) {
    int text_width = strlen(label) * FONT_WIDTH;

    surface_fill_rect(s, r.x, r.y, r.w, r.h, THEME_BUTTON);
    surface_draw_rect(s, r.x, r.y, r.w, r.h, THEME_BUTTON_BORDER);
    font_draw_text(s, r.x + (r.w - text_width) / 2, center_text_y(r.y, r.h), label, THEME_TEXT);
}

void widget_textbox(surface_t* s, rect_t r, const textbox_t* box, bool focused) {
    int visible_chars = (r.w - 2 * TEXT_PADDING) / FONT_WIDTH - 1;
    int length = strlen(box->text);
    /* If the text is too long, show its end, where the caret is. */
    const char* shown = box->text + (length > visible_chars ? length - visible_chars : 0);
    int text_x = r.x + TEXT_PADDING;
    int text_y = center_text_y(r.y, r.h);

    surface_fill_rect(s, r.x, r.y, r.w, r.h, THEME_FIELD);
    surface_draw_rect(s, r.x, r.y, r.w, r.h, focused ? THEME_TITLE_ACTIVE : THEME_BUTTON_BORDER);
    font_draw_text(s, text_x, text_y, shown, THEME_TEXT);
    if (focused) {
        int caret_x = text_x + strlen(shown) * FONT_WIDTH;
        surface_fill_rect(s, caret_x, text_y, CARET_WIDTH, FONT_HEIGHT, THEME_TEXT);
    }
}

bool widget_textbox_key(textbox_t* box, char key) {
    int length = strlen(box->text);

    if (key == '\b') {
        if (length == 0) {
            return false;
        }
        box->text[length - 1] = '\0';
        return true;
    }
    if (key < PRINTABLE_FIRST || key > PRINTABLE_LAST || length >= box->capacity - 1) {
        return false;
    }
    box->text[length] = key;
    box->text[length + 1] = '\0';
    return true;
}

void widget_list(surface_t* s, rect_t r, const char* const* items, int count,
                 int first, int selected) {
    int rows = r.h / WIDGET_LIST_ROW_HEIGHT;

    surface_fill_rect(s, r.x, r.y, r.w, r.h, THEME_FIELD);
    for (int row = 0; row < rows && first + row < count; row++) {
        int index = first + row;
        int y = r.y + row * WIDGET_LIST_ROW_HEIGHT;

        if (index == selected) {
            surface_fill_rect(s, r.x, y, r.w, WIDGET_LIST_ROW_HEIGHT, THEME_SELECTION);
        }
        font_draw_text(s, r.x + TEXT_PADDING, center_text_y(y, WIDGET_LIST_ROW_HEIGHT),
                       items[index], THEME_TEXT);
    }
    surface_draw_rect(s, r.x, r.y, r.w, r.h, THEME_BUTTON_BORDER);
}

int widget_list_hit(rect_t r, int count, int first, int x, int y) {
    if (!rect_contains(r, x, y)) {
        return -1;
    }
    int index = first + (y - r.y) / WIDGET_LIST_ROW_HEIGHT;
    return index < count ? index : -1;
}
