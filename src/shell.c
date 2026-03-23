#include "shell.h"
#include "strings.h"
#include "keyboard.h"
#include "util.h"
#include "kmalloc.h"
#include "fat12.h"

#define CAT_BUF_SIZE (512 * 16)   /* 8 KB — adjust if you need bigger files */

static bool is_running = true;

/* ── helpers ──────────────────────────────────────────── */

/* Skip leading spaces, return pointer to first non-space. */
static const char *ltrim(const char *s) {
    while (*s == ' ') s++;
    return s;
}

/* Return 1 if 'cmd' starts with 'prefix' followed by a space or NUL. */
static int starts_with(const char *cmd, const char *prefix) {
    uint32_t n = strlen(prefix);
    return strncmp(cmd, prefix, n) == 0 && (cmd[n] == ' ' || cmd[n] == '\0');
}

/* ── built-in commands ────────────────────────────────── */

static void cmd_info(void) {
    printf("OS: %s %s   Kernel: %s\n", OS_NAME, OS_VERSION, KERNEL_NAME);
}

static void cmd_help(void) {
    print("Available commands:\n");
    print("  info               - show OS info\n");
    print("  clear              - clear the screen\n");
    print("  echo <text>        - print text on the screen\n");
    print("  ls                 - list files\n");
    print("  cat <file>         - print file contents\n");
    print("  write <file> <txt> - write text to file\n");
    print("  del <file>         - delete a file\n");
    print("  help               - show this message\n");
}

static void cmd_clear(void) {
    vga_clear();
    printf_rainbow("%s V %s\n\n", OS_NAME, OS_VERSION);
}

static void cmd_del(const char *arg) {
    arg = ltrim(arg);
    if (!*arg) {
        print("Usage: del <filename>\n");
        return;
    }
    fat12_delete(arg);
}

static void cmd_echo(const char *arg) {
    arg = ltrim(arg);
    if (!*arg) {
        putc('\n');
        return;
    }
    printf("%s\n", arg);
}

static void cmd_cat(const char *arg) {
    arg = ltrim(arg);
    if (!*arg) {
        print("Usage: cat <filename>\n");
        return;
    }

    /* +1 so we can safely NUL-terminate up to CAT_BUF_SIZE bytes of content */
    uint8_t *buf = (uint8_t *)kmalloc(CAT_BUF_SIZE + 1);
    if (!buf) {
        print("cat: out of memory\n");
        return;
    }

    uint32_t size = fat12_read(arg, buf);
    if (!size) {
        printf("cat: file not found: %s\n", arg);
    } else {
        /* Clamp to buffer capacity before NUL-terminating */
        if (size > CAT_BUF_SIZE) size = CAT_BUF_SIZE;
        buf[size] = '\0';
        printf("%s\n", (char *)buf);
    }

    kfree(buf);
}

static void cmd_write(const char *arg) {
    arg = ltrim(arg);
    if (!*arg) {
        print("Usage: write <filename> <content>\n");
        return;
    }

    /* Find the space that separates filename from content */
    const char *p = arg;
    while (*p && *p != ' ') p++;

    if (!*p) {
        print("Usage: write <filename> <content>\n");
        return;
    }

    /* Copy filename into a small stack buffer */
    uint32_t name_len = (uint32_t)(p - arg);
    if (name_len >= 13) {
        print("write: filename too long (max 12 chars)\n");
        return;
    }
    char fname[13];
    for (uint32_t i = 0; i < name_len; i++) fname[i] = arg[i];
    fname[name_len] = '\0';

    const char *content = ltrim(p + 1);
    if (!*content) {
        print("write: no content provided\n");
        return;
    }

    fat12_write(fname, (uint8_t *)content, strlen(content));
}

/* ── main shell loop ──────────────────────────────────── */

void init_shell(void) {
    cmd_clear();

    char in[256];

    while (is_running) {
        print("> ");
        int len = readline(in);
        if (!len) continue;

        if (strcmp(in, "info") == 0) {
            cmd_info();
        } else if (strcmp(in, "help") == 0) {
            cmd_help();
        } else if (strcmp(in, "clear") == 0) {
            cmd_clear();
        } else if (strcmp(in, "ls") == 0) {
            fat12_list();
        } else if (starts_with(in, "echo")) {
            cmd_echo(in + 4);
        } else if (starts_with(in, "cat")) {
            cmd_cat(in + 3);
        } else if (starts_with(in, "write")) {
           cmd_write(in + 5);
        } else if (starts_with(in, "del")) {
            cmd_del(in + 3);
        }
        else {
            printf("Unknown command: %s\n", in);
            print("Type 'help' for a list of commands.\n");
        }
    }
}