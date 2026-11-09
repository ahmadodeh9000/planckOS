/*
 * keyboard.c - PS/2 keyboard on IRQ1, scan code set 1.
 *
 * Every key press or release makes the keyboard controller raise IRQ1 with
 * one scan code waiting in port 0x60. Press codes are below 0x80; the
 * release code of a key is its press code | 0x80. The handler turns press
 * codes into ASCII and pushes an EVENT_KEY for each one; whoever reads the
 * event queue (the shell, the GUI) decides what to do with it.
 */
#include "drivers/keyboard.h"
#include "arch/idt.h"
#include "gui/event.h"

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

static bool shift_pressed = false;

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
        event_t event = { .type = EVENT_KEY, .key = c };
        event_push(&event);
    }
}

void keyboard_init(void) {
    irq_install_handler(KEYBOARD_IRQ, keyboard_handler);
}
