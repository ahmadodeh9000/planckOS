/*
 * shell.c - a small command shell.
 *
 * Typed keys build up a line (with echo and backspace). On Enter the first
 * word is looked up in the command table and the command runs with the
 * rest of the line as its argument string. All output goes to the shell's
 * console_t.
 */
#include "shell/shell.h"
#include "shell/logo.h"
#include "gui/event.h"
#include "fs/fat12.h"
#include "mm/kmalloc.h"
#include "drivers/power.h"
#include "drivers/rtc.h"
#include "arch/idt.h"
#include "exec/elf.h"
#include "exec/program.h"
#include "lib/string.h"

#define CAT_BUF_SIZE        (512 * 16)  /* largest file 'cat' can show: 8 KB */
#define LS_MAX_FILES        512
#define MAX_FILENAME_LEN    12          /* "NAME.EXT" */
#define SHELL_LINE_MAX      256
#define PRINTABLE_FIRST     ' '
#define PRINTABLE_LAST      '~'
#define EXEC_MAX_FILE       (1024 * 1024)   /* largest program file 'exec' reads: 1 MB */

struct shell {
    console_t* con;
    char line[SHELL_LINE_MAX];
    uint32_t length;
};

typedef struct {
    const char* name;
    void (*run)(shell_t* sh, const char* args);
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

static void shell_banner(shell_t* sh) {
    logo_print(sh->con);
    console_print_rainbow(sh->con, OS_NAME " V " OS_VERSION);
    console_print(sh->con, "\n\n");
}

static void shell_prompt(shell_t* sh) {
    console_print(sh->con, "admin$ ");
}

/* ---- commands ----------------------------------------------------- */

static void cmd_info(shell_t* sh, const char* args) {
    (void)args;
    console_t* con = sh->con;
    console_print(con, "********************************************\n");
    console_print(con, "* AUTHOR:   Ahmad Odeh                     *\n");
    console_print(con, "* OS:       ");
    console_print_rainbow(con, OS_NAME);
    console_print(con, "                       *\n");
    console_printf(con, "* VERSION:  %s                          *\n", OS_VERSION);
    console_printf(con, "* KERNEL:   %s                           *\n", KERNEL_NAME);
    console_print(con, "********************************************\n");
}

static void cmd_help(shell_t* sh, const char* args) {
    (void)args;
    console_print(sh->con,
        "Available commands:\n"
        "  info               - show OS info\n"
        "  clear              - clear the screen\n"
        "  echo <text>        - print text on the screen\n"
        "  date               - show the date and time\n"
        "  ls                 - list files\n"
        "  cat <file>         - print file contents\n"
        "  write <file> <txt> - write text to file\n"
        "  del <file>         - delete a file\n"
        "  exec <file>        - run an ELF program\n"
        "  quit               - shutdown the os\n"
        "  help               - show this message\n");
}

static void cmd_clear(shell_t* sh, const char* args) {
    (void)args;
    console_clear(sh->con);
    shell_banner(sh);
}

static void cmd_echo(shell_t* sh, const char* args) {
    console_printf(sh->con, "%s\n", args);
}

static void cmd_date(shell_t* sh, const char* args) {
    (void)args;
    rtc_time_t now;
    char text[RTC_DATE_LEN + 1];

    rtc_read(&now);
    rtc_format_date(&now, text);
    console_printf(sh->con, "%s\n", text);
}

static void cmd_ls(shell_t* sh, const char* args) {
    (void)args;
    fat12_file_t* files = kmalloc(LS_MAX_FILES * sizeof(fat12_file_t));
    if (!files) {
        console_print(sh->con, "ls: out of memory\n");
        return;
    }
    int count = fat12_list(files, LS_MAX_FILES);
    for (int i = 0; i < count; i++) {
        console_printf(sh->con, "    %s  %u bytes\n", files[i].name, files[i].size);
    }
    kfree(files);
}

static void cmd_cat(shell_t* sh, const char* args) {
    if (!*args) {
        console_print(sh->con, "Usage: cat <filename>\n");
        return;
    }
    uint8_t* buf = kmalloc(CAT_BUF_SIZE + 1);   /* +1 for the '\0' */
    if (!buf) {
        console_print(sh->con, "cat: out of memory\n");
        return;
    }

    uint32_t size;
    fat12_status status = fat12_read(args, buf, CAT_BUF_SIZE, &size);
    if (status == FAT12_OK) {
        buf[size] = '\0';
        console_printf(sh->con, "%s\n", (char*)buf);
    } else {
        console_printf(sh->con, "cat: %s: %s\n", fat12_status_message(status), args);
    }
    kfree(buf);
}

/* write <file> <content> */
static void cmd_write(shell_t* sh, const char* args) {
    const char* space = args;
    while (*space && *space != ' ') {
        space++;
    }
    uint32_t name_len = space - args;
    const char* content = skip_spaces(space);
    if (!*args || !*content) {
        console_print(sh->con, "Usage: write <filename> <content>\n");
        return;
    }
    if (name_len > MAX_FILENAME_LEN) {
        console_print(sh->con, "write: filename too long (max 12 chars)\n");
        return;
    }

    char name[MAX_FILENAME_LEN + 1];
    memcpy(name, args, name_len);
    name[name_len] = '\0';
    uint32_t size = strlen(content);
    fat12_status status = fat12_write(name, (const uint8_t*)content, size);
    if (status == FAT12_OK) {
        console_printf(sh->con, "Wrote %s (%u bytes)\n", name, size);
    } else {
        console_printf(sh->con, "write: %s\n", fat12_status_message(status));
    }
}

static void cmd_del(shell_t* sh, const char* args) {
    if (!*args) {
        console_print(sh->con, "Usage: del <filename>\n");
        return;
    }
    fat12_status status = fat12_delete(args);
    if (status == FAT12_OK) {
        console_printf(sh->con, "deleted %s\n", args);
    } else {
        console_printf(sh->con, "%s\n", fat12_status_message(status));
    }
}

/* Read a whole program file into a new kmalloc'd buffer. On failure,
   print why and return NULL. */
static uint8_t* shell_read_program(shell_t* sh, const char* name, uint32_t* size) {
    /* One byte more than the limit, to notice files that are too big. */
    uint8_t* image = kmalloc(EXEC_MAX_FILE + 1);
    if (!image) {
        console_print(sh->con, "exec: out of memory\n");
        return NULL;
    }

    fat12_status status = fat12_read(name, image, EXEC_MAX_FILE + 1, size);
    if (status != FAT12_OK) {
        console_printf(sh->con, "exec: %s: %s\n", fat12_status_message(status), name);
    } else if (*size > EXEC_MAX_FILE) {
        console_printf(sh->con, "exec: %s is too big (the limit is 1 MB)\n", name);
    } else {
        return image;
    }
    kfree(image);
    return NULL;
}

/* exec <file>: load an ELF program and run it until it ends. */
static void cmd_exec(shell_t* sh, const char* args) {
    if (!*args) {
        console_print(sh->con, "Usage: exec <file>\n");
        return;
    }
    uint32_t size;
    uint8_t* image = shell_read_program(sh, args, &size);
    if (!image) {
        return;
    }

    uint32_t entry = elf_load(image, size);
    kfree(image);       /* the program now lives in the program area */
    if (!entry) {
        console_printf(sh->con, "exec: %s is not a valid planckOS program "
                                "(the serial log says why)\n", args);
        return;
    }

    program_result_t result = program_run(entry, sh->con);
    if (result.crashed) {
        console_printf(sh->con, "exec: %s crashed: %s\n", args,
                       idt_exception_name(result.exception));
    } else if (result.exit_code != 0) {
        console_printf(sh->con, "exec: %s exited with code %u\n", args, result.exit_code);
    }
}

static void cmd_quit(shell_t* sh, const char* args) {
    (void)sh;
    (void)args;
    power_off();
}

static const shell_command commands[] = {
    { "info",  cmd_info  },
    { "help",  cmd_help  },
    { "quit",  cmd_quit  },
    { "clear", cmd_clear },
    { "ls",    cmd_ls    },
    { "echo",  cmd_echo  },
    { "date",  cmd_date  },
    { "cat",   cmd_cat   },
    { "write", cmd_write },
    { "del",   cmd_del   },
    { "exec",  cmd_exec  },
};

#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

/* ---- line editing ------------------------------------------------- */

static void shell_execute(shell_t* sh, const char* line) {
    for (uint32_t i = 0; i < COMMAND_COUNT; i++) {
        if (is_command(line, commands[i].name)) {
            commands[i].run(sh, skip_spaces(line + strlen(commands[i].name)));
            return;
        }
    }
    console_printf(sh->con, "Unknown command: %s\n", line);
    console_print(sh->con, "Type 'help' for a list of commands.\n");
}

void shell_key(shell_t* sh, char key) {
    if (key == '\n') {
        console_putc(sh->con, '\n');
        sh->line[sh->length] = '\0';
        if (sh->length > 0) {
            shell_execute(sh, sh->line);
        }
        sh->length = 0;
        shell_prompt(sh);
    } else if (key == '\b') {
        if (sh->length > 0) {
            sh->length--;
            console_putc(sh->con, '\b');
        }
    } else if (key >= PRINTABLE_FIRST && key <= PRINTABLE_LAST &&
               sh->length < SHELL_LINE_MAX - 1) {
        /* Arrow keys and other KEY_* codes aren't printable: the shell has
           no line editing beyond backspace, so it ignores them. */
        sh->line[sh->length++] = key;
        console_putc(sh->con, key);
    }
}

shell_t* shell_create(console_t* con) {
    shell_t* sh = kmalloc(sizeof(shell_t));
    if (!sh) {
        return NULL;
    }
    sh->con = con;
    sh->length = 0;
    console_clear(con);
    console_putc(con, '\n');
    shell_banner(sh);
    shell_prompt(sh);
    return sh;
}

void shell_destroy(shell_t* sh) {
    kfree(sh);
}

void shell_run(console_t* con) {
    shell_t* sh = shell_create(con);
    event_t event;

    for (;;) {
        event_wait(&event);
        if (sh && event.type == EVENT_KEY) {
            shell_key(sh, event.key);
        }
    }
}
