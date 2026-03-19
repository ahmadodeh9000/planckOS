#include "keyboard.h"
#include "idt/idt.h"
#include "strings.h"

#define BUF_SIZE    2048

static char buffer[BUF_SIZE] = {};
bool input_ready = false;
static uint32_t index = 0;

static const char scancode_map[128] = {
    0,  0,  '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*', 0, ' '
};

static const char scancode_map_shift[128] = {
    0,  0,  '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?',0,
    '*', 0, ' '
};

static bool is_shift_pressed = false;


void keyboard_handler(registers_t* regs) {
    unsigned char scan_code = inportb(0x60); // what key is pressed


    /* checking left and right shift press/release */
    if (scan_code == 0x2A || scan_code == 0x36) { is_shift_pressed = true; return;} 
    if (scan_code == 0xAA || scan_code == 0xB6) { is_shift_pressed = false; return;}
    
    

    if (scan_code & 0x80) return; 

    char c = is_shift_pressed ? scancode_map_shift[scan_code] : scancode_map[scan_code];
    if (!c) return; // unmapped
    
    if (c == '\b') {
        if (index > 0) {
            --index;
            buffer[index] = '\0';
            putc('\b');
        }
       
        return;
    }

    if (index < BUF_SIZE - 1) {
        buffer[index++] = c;
        putc(c);
    }

    if (c == '\n') {
        buffer[index] = '\0';
        input_ready = true;
    }

}



uint32_t readline(char* s) {

    input_ready = false;
    index = 0;

    while (!input_ready) {
        // busy-wait: interrupts still fire and fill the buffer
        __asm__ volatile("hlt");            // sleep until next interrupt (optional but saves CPU)
    }

    uint32_t len = index;
    for (uint32_t i = 0; i <= len; i++)    // copy including '\0'
        s[i] = buffer[i];


    if (len > 0 && s[len - 1] == '\n') {
        s[len - 1] = '\0';
    }

    index = 0;
    input_ready = false;
    return len - 1;
}

void keyboard_init() {
    irq_install_handler(1,keyboard_handler);
}