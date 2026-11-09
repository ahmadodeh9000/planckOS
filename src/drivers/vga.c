/*
 * vga.c - VGA text-mode output.
 *
 * The text buffer at 0xB8000 holds 80x25 cells. Each cell is two bytes:
 * the character, then an attribute byte (background color << 4 | foreground
 * color). Console colors are VGA palette indexes, so they go straight into
 * the attribute. The blinking hardware cursor is moved through the CRT
 * controller registers at ports 0x3D4/0x3D5.
 */
#include "drivers/vga.h"
#include "arch/io.h"

#define VGA_MEMORY          0xB8000
#define VGA_BLACK           0
#define VGA_BLANK_COLOR     ((VGA_BLACK << 4) | CONSOLE_COLOR_DEFAULT)

#define CRTC_INDEX_PORT     0x3D4
#define CRTC_DATA_PORT      0x3D5
#define CRTC_CURSOR_HIGH    0x0E
#define CRTC_CURSOR_LOW     0x0F

static int cursor_row = 0;
static int cursor_col = 0;

static uint16_t vga_cell(char c, uint8_t color) {
    return ((uint16_t)color << 8) | (uint8_t)c;
}

static void vga_update_cursor(void) {
    uint16_t pos = cursor_row * VGA_WIDTH + cursor_col;

    outportb(CRTC_INDEX_PORT, CRTC_CURSOR_LOW);
    outportb(CRTC_DATA_PORT, pos & 0xFF);
    outportb(CRTC_INDEX_PORT, CRTC_CURSOR_HIGH);
    outportb(CRTC_DATA_PORT, (pos >> 8) & 0xFF);
}

static void vga_scroll(void) {
    uint16_t* vga = (uint16_t*)VGA_MEMORY;

    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        vga[i] = vga[i + VGA_WIDTH];
    }
    for (int col = 0; col < VGA_WIDTH; col++) {
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = vga_cell(' ', VGA_BLANK_COLOR);
    }
}

static void vga_newline(void) {
    cursor_row++;
    cursor_col = 0;
    if (cursor_row >= VGA_HEIGHT) {
        vga_scroll();
        cursor_row = VGA_HEIGHT - 1;
    }
}

/* Step back one cell (to the end of the previous row if needed) and blank it. */
static void vga_backspace(void) {
    if (cursor_col == 0 && cursor_row == 0) {
        return;
    }
    if (--cursor_col < 0) {
        cursor_col = VGA_WIDTH - 1;
        cursor_row--;
    }
    uint16_t* vga = (uint16_t*)VGA_MEMORY;
    vga[cursor_row * VGA_WIDTH + cursor_col] = vga_cell(' ', VGA_BLANK_COLOR);
}

static void vga_put_char(console_t* con, char c, uint8_t color) {
    (void)con;
    if (c == '\b') {
        vga_backspace();
    } else if (c == '\n') {
        vga_newline();
    } else {
        if (cursor_col >= VGA_WIDTH) {
            vga_newline();
        }
        uint16_t* vga = (uint16_t*)VGA_MEMORY;
        vga[cursor_row * VGA_WIDTH + cursor_col] = vga_cell(c, color);
        cursor_col++;
    }
    vga_update_cursor();
}

static void vga_clear(console_t* con) {
    (void)con;
    uint16_t* vga = (uint16_t*)VGA_MEMORY;

    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        vga[i] = vga_cell(' ', VGA_BLANK_COLOR);
    }
    cursor_col = 0;
    cursor_row = 0;
    vga_update_cursor();
}

static console_t vga_sink = { vga_put_char, vga_clear, NULL };

console_t* vga_console(void) {
    return &vga_sink;
}
