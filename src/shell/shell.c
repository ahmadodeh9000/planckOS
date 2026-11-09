/*
 * shell.c - a small command shell on the VGA text console.
 *
 * Reads a line with readline(), finds the command in the command table and
 * runs it with the rest of the line as its argument string.
 */
#include "shell/shell.h"
#include "drivers/keyboard.h"
#include "drivers/vga.h"
#include "fs/fat12.h"
#include "mm/kmalloc.h"
#include "arch/io.h"
#include "lib/printf.h"
#include "lib/string.h"

#define CAT_BUF_SIZE        (512 * 16)  /* largest file 'cat' can show: 8 KB */
#define MAX_FILENAME_LEN    12          /* "NAME.EXT" */

/* Writing 0x2000 to port 0x604 powers off QEMU (ACPI shutdown). */
#define QEMU_SHUTDOWN_PORT  0x604
#define QEMU_SHUTDOWN_VALUE 0x2000

typedef struct {
    const char* name;
    void (*run)(const char* args);
} shell_command;

static const char* skip_spaces(const char* s) {
    while (*s == ' ') {
        s++;
    }
    return s;
}

/* True if 'line' is 'name' alone or followed by a space. */
static bool is_command(const char* line, const char* name) {
    uint32_t n = strlen(name);
    return strncmp(line, name, n) == 0 && (line[n] == ' ' || line[n] == '\0');
}

static void cmd_info(const char* args) {
    (void)args;
    printf("********************************************\n");
    printf("* AUTHOR:   Ahmad Odeh                     *\n");
    printf_rainbow("* OS:       %s                       *\n", OS_NAME);
    printf("* VERSION:  %s                          *\n", OS_VERSION);
    printf("* KERNEL:   %s                           *\n", KERNEL_NAME);
    printf("********************************************\n");
}

static void cmd_help(const char* args) {
    (void)args;
    print("Available commands:\n");
    print("  info               - show OS info\n");
    print("  clear              - clear the screen\n");
    print("  echo <text>        - print text on the screen\n");
    print("  ls                 - list files\n");
    print("  cat <file>         - print file contents\n");
    print("  write <file> <txt> - write text to file\n");
    print("  del <file>         - delete a file\n");
    print("  quit               - shutdown the os\n");
    print("  help               - show this message\n");
}

static void cmd_clear(const char* args) {
    (void)args;
    vga_clear();
    printf_rainbow("%s V %s\n\n", OS_NAME, OS_VERSION);
}

static void cmd_ls(const char* args) {
    (void)args;
    fat12_list();
}

static void cmd_echo(const char* args) {
    printf("%s\n", args);
}

static void cmd_del(const char* args) {
    if (!*args) {
        print("Usage: del <filename>\n");
        return;
    }
    fat12_delete(args);
}

static void cmd_cat(const char* args) {
    if (!*args) {
        print("Usage: cat <filename>\n");
        return;
    }

    /* +1 so the file can be '\0'-terminated */
    uint8_t* buf = kmalloc(CAT_BUF_SIZE + 1);
    if (!buf) {
        print("cat: out of memory\n");
        return;
    }

    uint32_t size = fat12_read(args, buf);
    if (!size) {
        printf("cat: file not found: %s\n", args);
    } else {
        if (size > CAT_BUF_SIZE) {
            size = CAT_BUF_SIZE;
        }
        buf[size] = '\0';
        printf("%s\n", (char*)buf);
    }
    kfree(buf);
}

/* write <file> <content> */
static void cmd_write(const char* args) {
    const char* space = args;
    while (*space && *space != ' ') {
        space++;
    }
    if (!*args || !*space) {
        print("Usage: write <filename> <content>\n");
        return;
    }

    uint32_t name_len = space - args;
    if (name_len > MAX_FILENAME_LEN) {
        print("write: filename too long (max 12 chars)\n");
        return;
    }
    char name[MAX_FILENAME_LEN + 1];
    memcpy(name, args, name_len);
    name[name_len] = '\0';

    const char* content = skip_spaces(space + 1);
    if (!*content) {
        print("write: no content provided\n");
        return;
    }
    fat12_write(name, (uint8_t*)content, strlen(content));
}

static void cmd_quit(const char* args) {
    (void)args;
    outportw(QEMU_SHUTDOWN_PORT, QEMU_SHUTDOWN_VALUE);
    for (;;) {
        asm volatile("cli; hlt");
    }
}

static const shell_command commands[] = {
    { "info",  cmd_info  },
    { "help",  cmd_help  },
    { "quit",  cmd_quit  },
    { "clear", cmd_clear },
    { "ls",    cmd_ls    },
    { "echo",  cmd_echo  },
    { "cat",   cmd_cat   },
    { "write", cmd_write },
    { "del",   cmd_del   },
};

#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

static void shell_execute(const char* line) {
    for (uint32_t i = 0; i < COMMAND_COUNT; i++) {
        if (is_command(line, commands[i].name)) {
            commands[i].run(skip_spaces(line + strlen(commands[i].name)));
            return;
        }
    }
    printf("Unknown command: %s\n", line);
    print("Type 'help' for a list of commands.\n");
}

void shell_run(void) {
    char line[KEYBOARD_LINE_MAX];

    cmd_clear("");
    for (;;) {
        printf("> ");
        if (readline(line)) {
            shell_execute(line);
        }
    }
}
