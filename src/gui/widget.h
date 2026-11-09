/*
 * widget.h - tiny drawing and input helpers for app windows.
 *
 * Not a toolkit: there are no widget objects. Each helper draws one kind
 * of control into a surface at a rectangle the app chooses, and a few
 * helpers answer "what did this click/key do?". The app keeps the state
 * (text, selection) itself and decides when to repaint.
 */
#ifndef widget_h__
#define widget_h__

#include "lib/common.h"
#include "gfx/surface.h"

#define WIDGET_LIST_ROW_HEIGHT  20

/* Single-line text field. The app owns the buffer. */
typedef struct {
    char* text;         /* '\0'-terminated */
    int capacity;       /* size of 'text' in bytes, including the '\0' */
} textbox_t;

void widget_label(surface_t* s, int x, int y, const char* text);
void widget_button(surface_t* s, rect_t r, const char* label);

void widget_textbox(surface_t* s, rect_t r, const textbox_t* box, bool focused);

/* Apply a typed key (printable characters and backspace). Returns true if
   the text changed. */
bool widget_textbox_key(textbox_t* box, char key);

/* A vertical list of 'count' strings starting at row 'first' (for
   scrolling), with row 'selected' highlighted (-1 = none). */
void widget_list(surface_t* s, rect_t r, const char* const* items, int count,
                 int first, int selected);

/* Index of the list item at content position (x, y), or -1. */
int widget_list_hit(rect_t r, int count, int first, int x, int y);

#endif
