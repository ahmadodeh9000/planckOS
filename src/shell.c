#include "shell.h"
#include "strings.h"
#include "vga.h"
#include "keyboard.h"
#include "util.h"


static bool is_running = true;


void print_info() {
    print_rainbow(OS_NAME);
    print(" ");
    print(KERNEL_NAME);
    print(" V ");
    print(OS_VERSION);
    print("\n");
}

void help_shell() {
    print("     1) info\n");
    print("     2) clear\n");
}

void clear() {
    vga_clear();
    print_rainbow(OS_NAME);
    print_rainbow(" V ");
    print_rainbow(OS_VERSION);
    print("\n\n");
}

void init_shell() {

    print_rainbow(OS_NAME);
    print_rainbow(" V ");
    print_rainbow(OS_VERSION);
    print("\n\n");


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

        else {
            print("Unknown\n");
        }
        
    }
}