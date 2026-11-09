/*
 * vga.c - VGA text-mode output.
 *
 * The text buffer at 0xB8000 holds 80x25 cells. Each cell is two bytes:
 * the character, then an attribute byte (background color << 4 | foreground
 * color). The blinking hardware cursor is moved through the CRT controller
 * registers at ports 0x3D4/0x3D5.
 */
#include "drivers/vga.h"
#include "arch/io.h"

#define VGA_MEMORY          0xB8000
#define VGA_DEFAULT_COLOR   ((VGA_COLOR_BLACK << 4) | VGA_COLOR_WHITE)
#define RAINBOW_COLORS      15      /* foreground colors 1-15; 0 is black */

#define CRTC_INDEX_PORT     0x3D4
#define CRTC_DATA_PORT      0x3D5
#define CRTC_CURSOR_HIGH    0x0E
#define CRTC_CURSOR_LOW     0x0F

/* Backspace never moves into the banner the shell prints on the first two
   rows. */
#define BACKSPACE_STOP_ROW  2

static int32_t cursor_row = 0;
static int32_t cursor_col = 0;

static uint16_t vga_cell(char c, uint8_t color) {
    return ((uint16_t)color << 8) | (uint8_t)c;
}

static uint8_t rainbow_color(int32_t i) {
    return (i % RAINBOW_COLORS) + 1;
}

static void vga_update_cursor(int row, int col) {
    uint16_t pos = row * VGA_WIDTH + col;

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
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = vga_cell(' ', VGA_DEFAULT_COLOR);
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

void vga_clear(void) {
    uint16_t* vga = (uint16_t*)VGA_MEMORY;

    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        vga[i] = vga_cell(' ', VGA_DEFAULT_COLOR);
    }
    cursor_col = 0;
    cursor_row = 0;
}

static void print_char(char c, uint8_t color) {
    if (cursor_col >= VGA_WIDTH) {
        vga_newline();
    }

    if (c == '\n') {
        vga_newline();
    } else {
        uint16_t* vga = (uint16_t*)VGA_MEMORY;
        vga[cursor_row * VGA_WIDTH + cursor_col] = vga_cell(c, color);
        cursor_col++;
    }
    vga_update_cursor(cursor_row, cursor_col);
}

/* Erase the character before the cursor, painting the blank in 'color'. */
static void vga_backspace(uint8_t color) {
    if (cursor_col == 0 && cursor_row == BACKSPACE_STOP_ROW) {
        return;
    }

    --cursor_col;
    if (cursor_col < 0) {
        cursor_col = VGA_WIDTH - 1;
        --cursor_row;
    }
    print_char(' ', color);
    --cursor_col;
    vga_update_cursor(cursor_row, cursor_col);
}

void putc(char c) {
    if (c == '\b') {
        vga_backspace(VGA_COLOR_WHITE);
    } else {
        print_char(c, VGA_COLOR_WHITE);
    }
}

void putc_rainbow(char c) {
    if (c == '\b') {
        vga_backspace(rainbow_color(cursor_col));
    } else {
        print_char(c, VGA_COLOR_WHITE);
    }
}

void print(const char* str) {
    for (int32_t i = 0; str[i] != '\0'; ++i) {
        print_char(str[i], VGA_COLOR_WHITE);
    }
}

void print_rainbow(const char* str) {
    for (int32_t i = 0; str[i] != '\0'; ++i) {
        print_char(str[i], rainbow_color(i));
    }
}

/* Write 'number' in decimal into 'buf' (at least 12 bytes). */
static void format_decimal(int number, char* buf) {
    char digits[11];
    int count = 0;
    int len = 0;
    uint32_t value = number < 0 ? -(uint32_t)number : (uint32_t)number;

    do {
        digits[count++] = '0' + value % 10;
        value /= 10;
    } while (value);

    if (number < 0) {
        buf[len++] = '-';
    }
    while (count) {
        buf[len++] = digits[--count];
    }
    buf[len] = '\0';
}

/* Write 'number' as "0x..." hex into 'buf' (at least 11 bytes). */
static void format_hex(uint32_t number, char* buf) {
    const char* hex = "0123456789ABCDEF";
    char digits[8];
    int count = 0;
    int len = 0;

    do {
        digits[count++] = hex[number % 16];
        number /= 16;
    } while (number);

    buf[len++] = '0';
    buf[len++] = 'x';
    while (count) {
        buf[len++] = digits[--count];
    }
    buf[len] = '\0';
}

void print_number(int number) {
    char buf[12];
    format_decimal(number, buf);
    print(buf);
}

void print_hex(uint32_t number) {
    char buf[11];
    format_hex(number, buf);
    print(buf);
}

void print_number_rainbow(int number) {
    char buf[12];
    format_decimal(number, buf);
    print_rainbow(buf);
}

void print_hex_rainbow(uint32_t number) {
    char buf[11];
    format_hex(number, buf);
    print_rainbow(buf);
}
