#include "shell.h"
#include "strings.h"
#include "keyboard.h"
#include "util.h"
#include "kmalloc.h"
#include "fat32.h"

static bool is_running = true;


void print_info() {
        
    printf("OS: %s %s, Kernel : %s\n",OS_NAME,OS_VERSION,KERNEL_NAME);

}

void catt(const char* filename) {
    uint8_t* buf = (uint8_t*) kmalloc(512 * 16);
    uint32_t size = fat32_read_file(filename, buf);

    if (size) {
        buf[size] = '\0';
        printf("%s\n",buf);
    }

    kfree(buf);

}

void cat(const char* fname) {
    if (strlen(fname) == 4) return;

    for (uint32_t i = 5; fname[i] != '\0'; ++i) {
        putc(fname[i]);
    }
    putc('\n');
}

void echo(const char* str) {

    if (strlen(str) == 5) return;

    for (uint32_t i = 5; str[i] != '\0'; ++i) {
        putc(str[i]);
    }
    putc('\n');
}

const char* get_arg(const char* cmd, uint32_t cmd_len) {
    return (cmd + cmd_len + 1);
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
    print("     4) ls\n");
    print("     5) cat <filename>\n");
}

void clear() {
    vga_clear();
    
    printf_rainbow("%s V %s\n\n",OS_NAME,OS_VERSION);
    
}

bool check_cat(const char* str) {
    const char* cat_str = "cat ";
    bool is_cat = true;

    for (int i = 0;  i < 4; ++i) {
        if (cat_str[i] != str[i]) {
            is_cat = false;
        }
    }

    return is_cat;
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

        else if (strcmp(in,"ls") == 0) {
            fat32_list_root();
        }

        else if (check_cat(in)) {
            
            catt(get_arg(in,3));
        }

        else if (strcmp(in,"clear") == 0) {
            clear();
        }
        else if (check_echo(in)) {
            echo(in);
        }

        else {
            print("Unknown Command !\n");
        }
        
    }
}