#include "vga.h"
#include "util.h"

/* cursor positions */
static int32_t index        = 0;
static int32_t cursor_row   = 0;
static int32_t cursor_col   = 0;


/*
void set_back_ground_color(uint8_t bg , uint8_t fg) {
    color = (bg << 4) | fg;
    char* vga = (char*) VGA_MEMORY;

    for (int32_t i = 0 ; i < 80 * 25 ; ++i) {
        vga[i * 2] = ' ';
        vga[i * 2 + 1] = color;
    }
}
*/

static void vga_scroll() {
    uint16_t* vga = (uint16_t*) VGA_MEMORY;
    uint8_t clr = (VGA_COLOR_BLACK << 4) | VGA_COLOR_WHITE;

    for (int row = 1; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            vga[(row - 1) * VGA_WIDTH + col] = vga[row * VGA_WIDTH + col];
        }
    }

    for (int col = 0; col < VGA_WIDTH; col++) {
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = (clr << 8) | ' ';
    }
}

void vga_clear() {
    uint16_t* vga = (uint16_t*) VGA_MEMORY;
    uint8_t clr = (VGA_COLOR_BLACK << 4) | VGA_COLOR_WHITE; /* white on black*/

    for (uint32_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        vga[i] = (clr << 8) | ' ';
    }

    cursor_col = 0;
    cursor_row = 0;
}

void vga_unpdate_cursor(int row, int col) {
    uint16_t pos = row * VGA_WIDTH + col;

    uint8_t low = pos & 0xFF;
    uint8_t high= (pos >> 8) & 0xFF;

    outportb(0x3D4,0x0F);
    outportb(0x3D5,low);

    outportb(0x3D4,0x0E);
    outportb(0x3D5,high);
}

/* prints a single char */
void print_char(const char c,uint8_t clr) {

    if (cursor_col >= VGA_WIDTH) {
        cursor_row++;
        cursor_col = 0;
        
    }

    if (cursor_row >= VGA_HEIGHT) {
        vga_scroll();
        cursor_row = VGA_HEIGHT - 1;
    }

    if(c == '\n') {
        cursor_row ++;
        cursor_col = 0;

        if (cursor_row >= VGA_HEIGHT) {
            vga_scroll();
            cursor_row = VGA_HEIGHT - 1;
        }

        vga_unpdate_cursor(cursor_row,cursor_col);
        

        return;
    }
    
    char* vga = (char*) VGA_MEMORY;
    index = VGA_WIDTH * cursor_row + cursor_col;
    
    vga[index * 2] = c;
    vga[index * 2 + 1] = clr;
    
    cursor_col++;
    vga_unpdate_cursor(cursor_row,cursor_col);

    
}

void print_number(int number) {
    char buffer[12];
    int32_t index = 0;

    if (number == 0) {
        print_char('0',VGA_COLOR_WHITE);
        return;
    }

    if (number < 0) {
        print_char('-',VGA_COLOR_WHITE);
        number *= -1;
    }

    while (number > 0) {
        buffer[index++] = '0' + (number % 10);
        number /= 10;
    }

    for (int32_t k = index - 1; k >= 0; --k) {
        print_char(buffer[k],VGA_COLOR_WHITE);
    }
}

void print_hex(uint32_t num) {
    char hex[] = "0123456789ABCDEF";
    char buf[9];
    int i = 0;

    if (num == 0) {
        print("0x0");
        return;
    }

    while (num > 0) {
        buf[i++] = hex[num % 16];
        num /= 16;
    }

    print("0x");

    while (i--) {
        putc(buf[i]);
    }
}

/* for the keyboard */
void putc(char c) {

    if (c == '\b') {    /* for backspace */

        if (cursor_col == 0 && cursor_row == 2) {
            
            return;
        }
        --cursor_col;

        if (cursor_col < 0) {
            cursor_col = VGA_WIDTH - 1;
            --cursor_row;
        }

        print_char(' ',VGA_COLOR_WHITE);
        --cursor_col;
        
        
    }
    else {
        print_char(c,VGA_COLOR_WHITE);
    }

    vga_unpdate_cursor(cursor_row,cursor_col);
    
}

void print_number_rainbow(int number) {
    char buffer[12];
    int32_t index = 0;

    if (number == 0) {
        print_char('0',(index % 15) + 1);
        return;
    }

    if (number < 0) {
        print_char('-',(index % 15) + 1);
        number *= -1;
    }

    while (number > 0) {
        buffer[index++] = '0' + (number % 10);
        number /= 10;
    }

    for (int32_t k = index - 1; k >= 0; --k) {
        print_char(buffer[k],(index % 15) + 1);
    }
}
void print_hex_rainbow(uint32_t num) {
    char hex[] = "0123456789ABCDEF";
    char buf[9];
    int i = 0;

    if (num == 0) {
        print_rainbow("0x0");
        return;
    }

    while (num > 0) {
        buf[i++] = hex[num % 16];
        num /= 16;
    }

    print_rainbow("0x");

    while (i--) {
        putc_rainbow(buf[i]);
    }
}

void putc_rainbow(char c) {

    if (c == '\b') {    /* for backspace */

        if (cursor_col == 0 && cursor_row == 2) {
            
            return;
        }
        --cursor_col;

        if (cursor_col < 0) {
            cursor_col = VGA_WIDTH - 1;
            --cursor_row;
        }

        print_char(' ', (cursor_col % 15) + 1);
        --cursor_col;
        
        
    }
    else {
        print_char(c,VGA_COLOR_WHITE);
    }

    vga_unpdate_cursor(cursor_row,cursor_col);
    
}


/* prints a whole string */
void print(const char* str) {
    for (int32_t i = 0 ; str[i] != '\0'; ++i) {
       
        print_char(str[i],VGA_COLOR_WHITE);
    }
}

void print_rainbow(const char* str) {
    for (int32_t i = 0 ; str[i] != '\0'; ++i) {
        print_char(str[i],(i % 15) + 1);
    }
}

