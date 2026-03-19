#include "shell.h"
#include "strings.h"
#include "keyboard.h"
#include "util.h"


static bool is_running = true;


void print_info() {
        
    printf("OS: %s %s, Kernel : %s\n",OS_NAME,OS_VERSION,KERNEL_NAME);

}

void echo(const char* str) {

    if (strlen(str) == 5) return;

    for (uint32_t i = 5; str[i] != '\0'; ++i) {
        putc(str[i]);
    }
    putc('\n');
}

bool check_echo(const char* str) {
    bool is_echo = true;
    char* echo_str = "echo ";

    for (int i = 0; i < 5; ++i) {
        if (str[i] != echo_str[i]) {
            is_echo = false;
        }
    }

    return is_echo;
}

void help_shell() {
    print("     1) info\n");
    print("     2) clear\n");
    print("     3) echo\n");
}

void clear() {
    vga_clear();
    
    printf_rainbow("%s V %s\n\n",OS_NAME,OS_VERSION);
    
}

void init_shell() {

    clear();


    char in[256];
    while (is_running) {
        print("> ");
        int len = readline(in);

        if (!len) continue;

        if (strcmp(in,"info") == 0) {
            print_info();
        }

        else if (strcmp(in,"help") == 0) {
            help_shell();
        }

        else if (strcmp(in,"clear") == 0) {
            clear();
        }
        else if (check_echo(in)) {
            echo(in);
        }

        else {
            print("Unknown\n");
        }
        
    }
}