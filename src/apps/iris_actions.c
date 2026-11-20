/*
 * iris_actions.c - running Iris's tools with real data from the kernel.
 *
 * Each tool writes a one-line answer for her bubble and the details for
 * the card under it. Nothing here comes from the model: she only chose
 * the tool and copied the values (iris_tools.c).
 */
#include "apps/iris_tools.h"
#include "apps/iris_templates.h"
#include "apps/notepad.h"
#include "asm/asm.h"
#include "fs/fat12.h"
#include "mm/kmalloc.h"
#include "mm/pmm.h"
#include "drivers/rtc.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include "lib/string.h"

#define KB              1024
#define PAGE_KB         4
#define FILES_MAX       512         /* the most files a listing reads */
#define FILES_SHOWN     10          /* the most shown in a card */
#define FIND_FILE_MAX   (64 * KB)   /* bigger files aren't searched */
#define FIND_RESULTS    6
#define FIND_LINE_SHOWN 36          /* characters of a matching line */
#define PROGRAM_MAX     (16 * KB)   /* an assembled template is ~1 KB */
#define CODE_LINES      10          /* lines of a new app shown in the card */
#define NAME_DIGITS     8           /* NAME2 ... NAME9 if NAME is taken */

/* ---- disk, memory, time, files ------------------------------------- */

static int count_files(void) {
    fat12_file_t* files = kmalloc(FILES_MAX * sizeof(fat12_file_t));
    int count = files ? fat12_list(files, FILES_MAX) : 0;
    kfree(files);
    return count;
}

static void run_disk(console_t* bubble, console_t* card) {
    uint32_t total, free;
    if (fat12_space(&total, &free) != FAT12_OK) {
        console_print(bubble, "i can't find a disk, sorry!");
        return;
    }
    console_printf(bubble, "you have %u KB free on the disk.", free / KB);
    console_printf(card, "used:  %u KB\nfree:  %u KB\ntotal: %u KB\nfiles: %d",
                   (total - free) / KB, free / KB, total / KB, count_files());
}

static void run_memory(console_t* bubble, console_t* card) {
    uint32_t pages, pages_used, heap_used, heap_free;
    pmm_stats(&pages, &pages_used);
    heap_stats(&heap_used, &heap_free);
    console_printf(bubble, "%u KB of the heap is free right now.", heap_free / KB);
    console_printf(card, "RAM:  %u KB used of %u KB\nheap: %u KB used, %u KB free\n"
                         "(I use about 6 MB of the heap myself!)",
                   pages_used * PAGE_KB, pages * PAGE_KB, heap_used / KB, heap_free / KB);
}

static void run_time(console_t* bubble, console_t* card) {
    rtc_time_t now;
    char clock[RTC_CLOCK_LEN + 1], day[RTC_DAY_LEN + 1], uptime[TIMER_UPTIME_LEN + 1];
    rtc_read(&now);
    rtc_format_clock(&now, clock);
    rtc_format_day(&now, day);
    timer_format_uptime(uptime);
    console_printf(bubble, "it's %s on %s.", clock, day);
    console_printf(card, "uptime: %s", uptime);
}

static void run_files(console_t* bubble, console_t* card) {
    fat12_file_t* files = kmalloc(FILES_MAX * sizeof(fat12_file_t));
    if (!files) {
        console_print(bubble, "i ran out of memory, sorry!");
        return;
    }
    int count = fat12_list(files, FILES_MAX);
    console_printf(bubble, count ? "you have %d files:" : "the disk is empty.", count);
    for (int i = 0; i < count && i < FILES_SHOWN; i++) {
        console_printf(card, "%s%s  %u bytes", i ? "\n" : "", files[i].name, files[i].size);
    }
    if (count > FILES_SHOWN) {
        console_printf(card, "\n... and %d more (the Files app has them all)", count - FILES_SHOWN);
    }
    kfree(files);
}

/* ---- find ---------------------------------------------------------- */

static char lower(char c) {
    return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
}

/* Case-insensitive search of 'needle' in data[0..size). */
static int find_in(const char* data, int size, const char* needle) {
    int n = strlen(needle);
    for (int i = 0; i + n <= size; i++) {
        int j = 0;
        while (j < n && lower(data[i + j]) == lower(needle[j])) {
            j++;
        }
        if (j == n) {
            return i;
        }
    }
    return -1;
}

/* Text files only: no bytes outside printable ASCII, tab and newlines. */
static bool is_text(const uint8_t* data, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        uint8_t c = data[i];
        if ((c < ' ' && c != '\n' && c != '\r' && c != '\t') || c > '~') {
            return false;
        }
    }
    return true;
}

/* Print the line around data[at] (or as much of it as fits). */
static void print_line_at(console_t* card, const char* data, int size, int at) {
    int start = at, end = at;
    while (start > 0 && data[start - 1] != '\n') {
        start--;
    }
    while (end < size && data[end] != '\n' && data[end] != '\r' && end - start < FIND_LINE_SHOWN) {
        end++;
    }
    console_print(card, ": ");
    for (int i = start; i < end; i++) {
        console_putc(card, data[i] == '\t' ? ' ' : data[i]);
    }
}

/* Does this file match by name or by text? Prints one result line. */
static bool find_in_file(const fat12_file_t* file, const char* needle, uint8_t* buffer,
                         console_t* card, bool first) {
    bool by_name = find_in(file->name, strlen(file->name), needle) >= 0;
    int at = -1;
    uint32_t got = 0;
    if (file->size <= FIND_FILE_MAX && fat12_read(file->name, buffer, FIND_FILE_MAX, &got) == FAT12_OK &&
        is_text(buffer, got)) {
        at = find_in((const char*)buffer, got, needle);
    }
    if (!by_name && at < 0) {
        return false;
    }
    console_printf(card, "%s%s", first ? "" : "\n", file->name);
    if (at >= 0) {
        print_line_at(card, (const char*)buffer, got, at);
    }
    return true;
}

static void run_find(const char* needle, console_t* bubble, console_t* card) {
    fat12_file_t* files = kmalloc(FILES_MAX * sizeof(fat12_file_t));
    uint8_t* buffer = kmalloc(FIND_FILE_MAX);
    if (!files || !buffer) {
        console_print(bubble, "i ran out of memory, sorry!");
    } else {
        int count = fat12_list(files, FILES_MAX);
        int found = 0;
        for (int i = 0; i < count && found < FIND_RESULTS; i++) {
            found += find_in_file(&files[i], needle, buffer, card, found == 0);
        }
        console_printf(bubble, found ? "here's where i found \"%s\":" :
                                       "i couldn't find \"%s\" in any file name or text file.",
                       needle);
    }
    kfree(files);
    kfree(buffer);
}

/* ---- open ---------------------------------------------------------- */

static void run_open(const char* name, const tool_env_t* env, console_t* bubble) {
    uint32_t size;
    if (fat12_size(name, &size) != FAT12_OK) {
        console_printf(bubble, "i couldn't find %s on the disk. try asking me to find it?", name);
    } else if (!env->can_open_windows) {
        console_printf(bubble, "i can open files in the Iris app. here, try `cat %s`.", name);
    } else {
        notepad_open_file(name);
        console_printf(bubble, "opening %s in Notepad.", name);
    }
}

/* ---- make-app ------------------------------------------------------ */

static const char* slot_value(const tool_call_t* call, const char* key) {
    for (int i = 0; i < call->slot_count; i++) {
        if (strcmp(call->slots[i].key, key) == 0) {
            return call->slots[i].value;
        }
    }
    return NULL;
}

static bool name_taken(const char* base, const char* extension) {
    char name[FAT12_NAME_MAX];
    uint32_t size;
    strcpy(name, base);
    strcat(name, extension);
    return fat12_size(name, &size) == FAT12_OK;
}

/* A base name whose .ASM and .ELF don't exist yet, so Iris never
   overwrites your files: NAME, then NAME2 ... NAME9. */
static bool free_base_name(char* base) {
    char candidate[FAT12_NAME_MAX];
    for (int i = 1; i <= 9; i++) {
        strcpy(candidate, base);
        if (i > 1) {
            int n = strlen(candidate);
            n = n < NAME_DIGITS ? n : NAME_DIGITS - 1;     /* room for the digit */
            candidate[n] = '0' + i;
            candidate[n + 1] = '\0';
        }
        if (!name_taken(candidate, ".ASM") && !name_taken(candidate, ".ELF")) {
            strcpy(base, candidate);
            return true;
        }
    }
    return false;
}

/* The program from _start on, CODE_LINES lines of it. */
static void print_code(console_t* card, const char* source, const char* asm_name) {
    const char* p = source;
    while (*p && strncmp(p, "_start:", 7) != 0) {
        p++;
    }
    for (int line = 0; *p && line < CODE_LINES; p++) {
        console_putc(card, *p);
        line += *p == '\n';
    }
    console_printf(card, "    ...\n(all of it is in %s: open it in Notepad to change it)", asm_name);
}

/* Assemble 'source' and save NAME.ASM and NAME.ELF. */
static bool build_app(const char* base, const char* source, tool_env_t* env, console_t* bubble) {
    uint8_t* program = kmalloc(PROGRAM_MAX);
    char asm_name[FAT12_NAME_MAX], elf_name[FAT12_NAME_MAX];
    strcpy(asm_name, base);
    strcat(asm_name, ".ASM");
    strcpy(elf_name, base);
    strcat(elf_name, ".ELF");

    bool ok = false;
    asm_result_t result = program ? asm_assemble(source, strlen(source), program, PROGRAM_MAX)
                                  : (asm_result_t){ false, 0, 0, "out of memory" };
    if (!result.ok) {
        /* The values were checked, so this is a bug in the template. */
        serial_printf("iris: template for %s doesn't assemble: line %d: %s\n", base,
                      result.line, result.message);
        console_printf(bubble, "oops, my template has a bug: asm says line %d: %s",
                       result.line, result.message);
    } else if (fat12_write(asm_name, (const uint8_t*)source, strlen(source)) != FAT12_OK ||
               fat12_write(elf_name, program, result.size) != FAT12_OK) {
        console_print(bubble, "i couldn't save the app: is the disk full?");
    } else {
        strcpy(env->program, elf_name);
        ok = true;
    }
    kfree(program);
    return ok;
}

static void run_make_app(const tool_call_t* call, tool_env_t* env, console_t* bubble,
                         console_t* card) {
    char base[FAT12_NAME_MAX];
    template_request_t request = { call->value, base, slot_value(call, "title"),
                                   slot_value(call, "text"), slot_value(call, "color"),
                                   slot_value(call, "size") };
    template_base_name(&request, base);
    char* source = kmalloc(TEMPLATE_SOURCE_MAX);
    const char* problem = NULL;
    if (!source) {
        problem = "i ran out of memory";
    } else if (!free_base_name(base)) {
        problem = "every name i tried is taken";
    } else {
        problem = template_fill(&request, source, TEMPLATE_SOURCE_MAX);
    }
    if (problem) {
        console_printf(bubble, "i can't make that one: %s.", problem);
    } else if (build_app(base, source, env, bubble)) {
        console_printf(bubble, env->can_open_windows
                           ? "i made %s.ELF from the %s template. press Run to start it!"
                           : "i made %s.ELF from the %s template. type `exec %s.ELF` to start it!",
                       base, call->value, base);
        char asm_name[FAT12_NAME_MAX];
        strcpy(asm_name, base);
        strcat(asm_name, ".ASM");
        print_code(card, source, asm_name);
    }
    kfree(source);
}

/* ---- dispatch ------------------------------------------------------ */

void tool_run(const tool_call_t* call, tool_env_t* env, console_t* bubble, console_t* card) {
    env->program[0] = '\0';
    switch (call->kind) {
        case TOOL_DISK:     run_disk(bubble, card); break;
        case TOOL_MEMORY:   run_memory(bubble, card); break;
        case TOOL_TIME:     run_time(bubble, card); break;
        case TOOL_FILES:    run_files(bubble, card); break;
        case TOOL_FIND:     run_find(call->value, bubble, card); break;
        case TOOL_OPEN:     run_open(call->value, env, bubble); break;
        case TOOL_MAKE_APP: run_make_app(call, env, bubble, card); break;
        default:            console_print(bubble, TOOL_BAD_TAG_REPLY); break;
    }
}
