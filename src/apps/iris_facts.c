/*
 * iris_facts.c - fact cards: exact help for commands and apps Iris is asked about.
 *
 * Iris is a tiny model, and tiny models sometimes ramble or get a detail
 * wrong. So whenever your message names a shell command or an app, a card
 * with the facts appears under her reply: for a command, the exact line
 * from the shell's `help` (shell_help_line), for an app, a fixed
 * description. This is a plain keyword lookup, no AI involved: retrieval
 * in miniature (docs/LLM.md, "Fact cards").
 */
#include "apps/iris_facts.h"
#include "shell/shell.h"
#include "lib/string.h"

#define WORD_MAX    16

typedef struct {
    const char* name;           /* the word to look for, lowercase */
    const char* card;
} app_fact_t;

static const app_fact_t app_facts[] = {
    { "terminal", "Terminal - the command shell in a window (planckOS menu)" },
    { "files",    "Files - list, open and delete files (planckOS menu)" },
    { "notepad",  "Notepad - edit and save text files, up to 8 KB (planckOS menu)" },
    { "clock",    "Clock - an analog clock (planckOS menu)" },
    { "about",    "About - version, uptime and memory use (planckOS menu)" },
};

#define APP_FACT_COUNT ((int)(sizeof(app_facts) / sizeof(app_facts[0])))

/* Other words for a command: 'chat' is listed under 'iris'. */
static const char* command_alias(const char* word) {
    return strcmp(word, "chat") == 0 ? "iris" : word;
}

static const char* card_for(const char* word) {
    for (int i = 0; i < APP_FACT_COUNT; i++) {
        if (strcmp(word, app_facts[i].name) == 0) {
            return app_facts[i].card;
        }
    }
    return shell_help_line(command_alias(word));
}

static bool is_word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
}

static char lower(char c) {
    return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
}

/* Copy the next word of 'text' (lowercased) into 'word'; returns where
   the scan stopped, or NULL at the end. */
static const char* next_word(const char* text, char* word) {
    while (*text && !is_word_char(lower(*text))) {
        text++;
    }
    if (!*text) {
        return NULL;
    }
    int n = 0;
    while (*text && is_word_char(lower(*text))) {
        if (n < WORD_MAX - 1) {
            word[n++] = lower(*text);
        }
        text++;
    }
    word[n] = '\0';
    return text;
}

int facts_print(const char* message, console_t* out) {
    const char* shown[FACTS_MAX];
    int count = 0;
    char word[WORD_MAX];

    while (count < FACTS_MAX && (message = next_word(message, word)) != NULL) {
        const char* card = card_for(word);
        bool repeat = false;
        for (int i = 0; i < count; i++) {
            repeat |= shown[i] == card;
        }
        if (card && !repeat) {
            console_printf(out, "%s%s", count ? "\n" : "", card);
            shown[count++] = card;
        }
    }
    return count;
}
