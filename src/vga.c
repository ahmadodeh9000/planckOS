#include "vga.h"

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

/* prints a single char */
void print_char(const char c,uint8_t clr) {
    
    if (cursor_col >= VGA_WIDTH) {
        cursor_row++;
        cursor_col = 0;
    }

    if(c == '\n') {
        cursor_row ++;
        cursor_col = 0;
        return;
    }
    
    char* vga = (char*) VGA_MEMORY;
    index = VGA_WIDTH * cursor_row + cursor_col;
    
    vga[index * 2] = c;
    vga[index * 2 + 1] = clr;
    
    cursor_col++;
    
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
    else print_char(c,VGA_COLOR_WHITE);
}

void print_hex(uint32_t hex) {
    /* todo */
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

