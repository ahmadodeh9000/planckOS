/*
 * keyboard.c - PS/2 keyboard on IRQ1, scan code set 1.
 *
 * Every key press or release makes the keyboard controller raise IRQ1 with
 * one scan code waiting in port 0x60. Press codes are below 0x80; the
 * release code of a key is its press code | 0x80. The handler turns press
 * codes into ASCII, echoes them and collects them into a line buffer that
 * readline() waits on.
 */
#include "drivers/keyboard.h"
#include "drivers/vga.h"
#include "arch/idt.h"

#define KEYBOARD_IRQ            1
#define KEYBOARD_DATA_PORT      0x60
#define SCANCODE_RELEASE_BIT    0x80
#define SCANCODE_TABLE_SIZE     128

#define SC_LEFT_SHIFT           0x2A
#define SC_RIGHT_SHIFT          0x36
#define SC_LEFT_SHIFT_RELEASE   (SC_LEFT_SHIFT | SCANCODE_RELEASE_BIT)
#define SC_RIGHT_SHIFT_RELEASE  (SC_RIGHT_SHIFT | SCANCODE_RELEASE_BIT)

static const char scancode_map[SCANCODE_TABLE_SIZE] = {
    0,  0,  '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*', 0, ' '
};

static const char scancode_map_shift[SCANCODE_TABLE_SIZE] = {
    0,  0,  '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?',0,
    '*', 0, ' '
};

/* Shared between the IRQ handler and readline(), hence volatile. */
static char line_buffer[KEYBOARD_LINE_MAX];
static volatile uint32_t line_length = 0;
static volatile bool line_ready = false;
static bool shift_pressed = false;

static void keyboard_add_char(char c) {
    if (c == '\b') {
        if (line_length > 0) {
            line_buffer[--line_length] = '\0';
            putc('\b');
        }
        return;
    }

    if (line_length < KEYBOARD_LINE_MAX - 1) {
        line_buffer[line_length++] = c;
        putc(c);
    }
    if (c == '\n') {
        line_buffer[line_length] = '\0';
        line_ready = true;
    }
}

static void keyboard_handler(registers_t* regs) {
    (void)regs;
    uint8_t scan_code = inportb(KEYBOARD_DATA_PORT);

    if (scan_code == SC_LEFT_SHIFT || scan_code == SC_RIGHT_SHIFT) {
        shift_pressed = true;
        return;
    }
    if (scan_code == SC_LEFT_SHIFT_RELEASE || scan_code == SC_RIGHT_SHIFT_RELEASE) {
        shift_pressed = false;
        return;
    }
    if (scan_code & SCANCODE_RELEASE_BIT) {
        return;
    }

    char c = shift_pressed ? scancode_map_shift[scan_code] : scancode_map[scan_code];
    if (c) {
        keyboard_add_char(c);
    }
}

uint32_t readline(char* s) {
    line_ready = false;
    line_length = 0;

    while (!line_ready) {
        asm volatile("hlt");    /* sleep until the next interrupt */
    }

    uint32_t len = line_length;
    for (uint32_t i = 0; i <= len; i++) {   /* copy including '\0' */
        s[i] = line_buffer[i];
    }
    if (len > 0 && s[len - 1] == '\n') {
        s[len - 1] = '\0';
    }

    line_length = 0;
    line_ready = false;
    return len - 1;
}

void keyboard_init(void) {
    irq_install_handler(KEYBOARD_IRQ, keyboard_handler);
}
