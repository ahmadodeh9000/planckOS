#include "keyboard.h"
#include "idt.h"
#include "vga.h"

static const char scancode_map[128] = {
    0,  0,  '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*', 0, ' '
};

void keyboard_handler(registers_t* regs) {
    char scan_code = inportb(0x60);// what key is pressed
    

    if (scan_code & 0x80) return; 

    char c = scancode_map[scan_code];

    if (c) {
        putc(c);
    }
}

void init_keyboard() {
    irq_install_handler(1,keyboard_handler);
}