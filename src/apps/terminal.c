/*
 * terminal.c - the Terminal app: the shell in a window.
 *
 * The window holds an 80x24 grid of character cells. The grid is a
 * console_t sink, so the shell prints into it exactly as it prints to the
 * VGA text screen. Typed keys go to shell_key(); afterwards the window is
 * repainted from the grid.
 */
#include "apps/terminal.h"
#include "gui/wm.h"
#include "shell/shell.h"
#include "gfx/font.h"
#include "mm/kmalloc.h"
#include "lib/string.h"

#define TERM_COLS       80
#define TERM_ROWS       24
#define TERM_PADDING    4
#define TERM_BACKGROUND RGB(0x00, 0x00, 0x00)
#define CURSOR_HEIGHT   2

/* The 16 standard VGA text colors, so shell colors look the same in
   text mode and in the window. */
static const uint32_t palette[CONSOLE_COLOR_COUNT] = {
    RGB(0x00, 0x00, 0x00), RGB(0x00, 0x00, 0xAA), RGB(0x00, 0xAA, 0x00), RGB(0x00, 0xAA, 0xAA),
    RGB(0xAA, 0x00, 0x00), RGB(0xAA, 0x00, 0xAA), RGB(0xAA, 0x55, 0x00), RGB(0xAA, 0xAA, 0xAA),
    RGB(0x55, 0x55, 0x55), RGB(0x55, 0x55, 0xFF), RGB(0x55, 0xFF, 0x55), RGB(0x55, 0xFF, 0xFF),
    RGB(0xFF, 0x55, 0x55), RGB(0xFF, 0x55, 0xFF), RGB(0xFF, 0xFF, 0x55), RGB(0xFF, 0xFF, 0xFF),
};

typedef struct {
    console_t console;              /* console.data points back here */
    char chars[TERM_ROWS][TERM_COLS];
    uint8_t colors[TERM_ROWS][TERM_COLS];
    int row, col;                   /* cursor */
    shell_t* shell;
} terminal_state;

/* ---- the grid as a console sink ----------------------------------- */

static void grid_clear_row(terminal_state* t, int row) {
    memset(t->chars[row], ' ', TERM_COLS);
    memset(t->colors[row], CONSOLE_COLOR_DEFAULT, TERM_COLS);
}

static void grid_newline(terminal_state* t) {
    t->col = 0;
    if (++t->row < TERM_ROWS) {
        return;
    }
    /* Scroll everything up one row. */
    memcpy(t->chars[0], t->chars[1], (TERM_ROWS - 1) * TERM_COLS);
    memcpy(t->colors[0], t->colors[1], (TERM_ROWS - 1) * TERM_COLS);
    grid_clear_row(t, TERM_ROWS - 1);
    t->row = TERM_ROWS - 1;
}

static void grid_backspace(terminal_state* t) {
    if (t->col == 0 && t->row == 0) {
        return;
    }
    if (--t->col < 0) {
        t->col = TERM_COLS - 1;
        t->row--;
    }
    t->chars[t->row][t->col] = ' ';
}

static void grid_put_char(console_t* con, char c, uint8_t color) {
    terminal_state* t = con->data;

    if (c == '\n') {
        grid_newline(t);
    } else if (c == '\b') {
        grid_backspace(t);
    } else {
        if (t->col >= TERM_COLS) {
            grid_newline(t);
        }
        t->chars[t->row][t->col] = c;
        t->colors[t->row][t->col] = color;
        t->col++;
    }
}

static void grid_clear(console_t* con) {
    terminal_state* t = con->data;

    for (int row = 0; row < TERM_ROWS; row++) {
        grid_clear_row(t, row);
    }
    t->row = 0;
    t->col = 0;
}

/* ---- window callbacks --------------------------------------------- */

static void terminal_paint(window_t* win) {
    terminal_state* t = win->data;
    surface_t* s = &win->surface;

    surface_fill_rect(s, 0, 0, s->width, s->height, TERM_BACKGROUND);
    for (int row = 0; row < TERM_ROWS; row++) {
        for (int col = 0; col < TERM_COLS; col++) {
            font_draw_char(s, TERM_PADDING + col * FONT_WIDTH, TERM_PADDING + row * FONT_HEIGHT,
                           t->chars[row][col], palette[t->colors[row][col]]);
        }
    }
    surface_fill_rect(s, TERM_PADDING + t->col * FONT_WIDTH,
                      TERM_PADDING + (t->row + 1) * FONT_HEIGHT - CURSOR_HEIGHT,
                      FONT_WIDTH, CURSOR_HEIGHT, palette[CONSOLE_COLOR_DEFAULT]);
}

static void terminal_key(window_t* win, char key) {
    terminal_state* t = win->data;
    shell_key(t->shell, key);
    window_invalidate(win);
}

static void terminal_close(window_t* win) {
    terminal_state* t = win->data;
    shell_destroy(t->shell);
    kfree(t);
}

void terminal_open(void) {
    terminal_state* t = kmalloc(sizeof(terminal_state));
    window_t* win = window_create("Terminal", TERM_COLS * FONT_WIDTH + 2 * TERM_PADDING,
                                  TERM_ROWS * FONT_HEIGHT + 2 * TERM_PADDING);
    if (!t || !win) {
        kfree(t);
        if (win) {
            window_close(win);
        }
        return;
    }

    t->console.put_char = grid_put_char;
    t->console.clear = grid_clear;
    t->console.data = t;
    grid_clear(&t->console);
    t->shell = shell_create(&t->console);
    if (!t->shell) {
        kfree(t);
        window_close(win);
        return;
    }

    win->data = t;
    win->on_paint = terminal_paint;
    win->on_key = terminal_key;
    win->on_close = terminal_close;
}
