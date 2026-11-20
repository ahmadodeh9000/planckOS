/*
 * iris_tools.c - deciding which tool to run: Iris's tag, or the fallback.
 *
 * Iris was trained (training/my_tools.txt) to answer requests like "is my
 * disk full" with a tag such as [disk]. tool_parse() reads the tag;
 * anything that isn't exactly one of the known forms is TOOL_BAD.
 *
 * A small model makes two kinds of mistakes here: it misses the tag (a
 * chatty answer to "how much space is left"), or it copies a value wrong
 * ([find "back"] for "find banana"). So tool_choose() trusts a tag only
 * if every value in it also appears in your message, and otherwise asks
 * the keyword fallback (tool_route), which looks for obvious phrases like
 * "find ...", "how much space" or "what time".
 *
 * Running the tools is in iris_actions.c.
 */
#include "apps/iris_tools.h"
#include "lib/string.h"

#define MESSAGE_MAX     128

/* ---- reading a tag ------------------------------------------------ */

typedef struct {
    const char* p;
} cursor_t;

static void skip_spaces(cursor_t* c) {
    while (*c->p == ' ') {
        c->p++;
    }
}

static bool is_lower(char ch) {
    return ch >= 'a' && ch <= 'z';
}

/* A word of lowercase letters and '-' ("make-app"). */
static bool read_word(cursor_t* c, char* out, int max) {
    int n = 0;
    while ((is_lower(*c->p) || *c->p == '-') && n < max - 1) {
        out[n++] = *c->p++;
    }
    out[n] = '\0';
    return n > 0 && !is_lower(*c->p);
}

/* "text": anything but '"', at least one character. */
static bool read_quoted(cursor_t* c, char* out, int max) {
    if (*c->p != '"') {
        return false;
    }
    c->p++;
    int n = 0;
    while (*c->p && *c->p != '"' && n < max - 1) {
        out[n++] = *c->p++;
    }
    out[n] = '\0';
    if (*c->p != '"' || n == 0) {
        return false;
    }
    c->p++;
    return true;
}

/* key="value" or key=word. */
static bool read_slot(cursor_t* c, tool_slot_t* slot) {
    if (!read_word(c, slot->key, sizeof(slot->key)) || *c->p != '=') {
        return false;
    }
    c->p++;
    if (*c->p == '"') {
        return read_quoted(c, slot->value, TOOL_VALUE_MAX);
    }
    return read_word(c, slot->value, TOOL_VALUE_MAX);
}

static tool_kind_t kind_of(const char* name) {
    static const struct { const char* name; tool_kind_t kind; } names[] = {
        { "disk", TOOL_DISK }, { "memory", TOOL_MEMORY }, { "time", TOOL_TIME },
        { "files", TOOL_FILES }, { "find", TOOL_FIND }, { "open", TOOL_OPEN },
        { "make-app", TOOL_MAKE_APP },
    };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (strcmp(name, names[i].name) == 0) {
            return names[i].kind;
        }
    }
    return TOOL_BAD;
}

/* What follows the tool's name, up to the ']'. */
static bool read_arguments(cursor_t* c, tool_call_t* call) {
    skip_spaces(c);
    if (call->kind == TOOL_FIND || call->kind == TOOL_OPEN) {
        if (!read_quoted(c, call->value, TOOL_VALUE_MAX)) {
            return false;
        }
    } else if (call->kind == TOOL_MAKE_APP) {
        if (!read_word(c, call->value, TOOL_VALUE_MAX)) {
            return false;
        }
        for (skip_spaces(c); *c->p != ']'; skip_spaces(c)) {
            if (call->slot_count == TOOL_SLOTS_MAX || !read_slot(c, &call->slots[call->slot_count++])) {
                return false;
            }
        }
    }
    skip_spaces(c);
    return *c->p == ']';
}

tool_kind_t tool_parse(const char* reply, tool_call_t* call) {
    memset(call, 0, sizeof(tool_call_t));
    cursor_t c = { reply };
    skip_spaces(&c);
    if (*c.p != '[') {
        return call->kind = TOOL_NONE;
    }
    c.p++;
    char name[TOOL_VALUE_MAX];
    read_word(&c, name, sizeof(name));
    call->kind = kind_of(name);
    if (call->kind == TOOL_BAD || !read_arguments(&c, call)) {
        return call->kind = TOOL_BAD;
    }
    c.p++;                          /* the ']' */
    skip_spaces(&c);
    return call->kind = (*c.p == '\0') ? call->kind : TOOL_BAD;
}

/* ---- the message, lowercased, and searching it --------------------- */

static char lower(char ch) {
    return (ch >= 'A' && ch <= 'Z') ? ch - 'A' + 'a' : ch;
}

static void to_lower(const char* in, char* out, int max) {
    int n = 0;
    for (; in[n] && n < max - 1; n++) {
        out[n] = lower(in[n]);
    }
    out[n] = '\0';
}

/* Where 'needle' starts in 'haystack' (both lowercase), or NULL. */
static const char* find_text(const char* haystack, const char* needle) {
    int n = strlen(needle);
    for (; *haystack; haystack++) {
        if (strncmp(haystack, needle, n) == 0) {
            return haystack;
        }
    }
    return NULL;
}

static bool is_letter(char ch) {
    return is_lower(ch) || (ch >= '0' && ch <= '9');
}

/* 'word' as a whole word in 'text' ("ram" but not "program"). */
static bool has_word(const char* text, const char* word) {
    int n = strlen(word);
    for (const char* p = text; (p = find_text(p, word)) != NULL; p++) {
        if ((p == text || !is_letter(p[-1])) && !is_letter(p[n])) {
            return true;
        }
    }
    return false;
}

static bool has_any_word(const char* text, const char* const* words) {
    for (; *words; words++) {
        if (has_word(text, *words)) {
            return true;
        }
    }
    return false;
}

/* Words that show a message is about each tool without a value. The
   keyword fallback looks for them too. */
static const char* const disk_words[] = { "disk", "storage", "space", "room", "full", NULL };
static const char* const memory_words[] = { "memory", "ram", "heap", NULL };
static const char* const time_words[] = { "time", "date", "day", "uptime", NULL };
static const char* const files_words[] = { "files", "file", "disk", "list", NULL };

/* A tag without values ([disk], [time], ...) is only believed if the
   message mentions what it's about: otherwise "what is the capital of
   spain" could get the disk space. */
static bool tool_has_evidence(const tool_call_t* call, const char* message) {
    char text[MESSAGE_MAX];
    to_lower(message, text, sizeof(text));
    switch (call->kind) {
        case TOOL_DISK:   return has_any_word(text, disk_words);
        case TOOL_MEMORY: return has_any_word(text, memory_words);
        case TOOL_TIME:   return has_any_word(text, time_words) || find_text(text, "how long");
        case TOOL_FILES:  return has_any_word(text, files_words);
        default:          return true;
    }
}

/* Is every value of the call in the message, as whole words? A value the
   model made up, or copied wrong ("Happy Frid" for "Happy Friday"), means
   the tag can't be trusted. */
static bool values_in_message(const tool_call_t* call, const char* message) {
    char text[MESSAGE_MAX];
    char value[TOOL_VALUE_MAX];
    to_lower(message, text, sizeof(text));

    if (call->kind == TOOL_FIND || call->kind == TOOL_OPEN) {
        to_lower(call->value, value, sizeof(value));
        return has_word(text, value);
    }
    for (int i = 0; i < call->slot_count; i++) {
        to_lower(call->slots[i].value, value, sizeof(value));
        if (!has_word(text, value)) {
            return false;
        }
    }
    return true;
}

tool_source_t tool_choose(const char* message, const char* reply, tool_call_t* call) {
    tool_kind_t kind = tool_parse(reply, call);
    if (kind != TOOL_NONE && kind != TOOL_BAD && values_in_message(call, message) &&
        tool_has_evidence(call, message)) {
        return TOOL_FROM_MODEL;
    }
    if (tool_route(message, call)) {
        return TOOL_FROM_FALLBACK;
    }
    return kind == TOOL_NONE ? TOOL_FROM_NOTHING : TOOL_FROM_BAD_TAG;
}

/* ---- the keyword fallback ------------------------------------------ */

/* Questions about how something works go to Iris, not to a tool. */
static const char* const how_to[] = { "how do", "how does", "what does", "command",
                                      "explain", "`", NULL };
static const char* const make_words[] = { "make", "create", "build", "want", "need", "give",
                                          NULL };
static const char* const filler[] = { "my", "for", "the", "a", "files", "file", "with",
                                      "about", "all", "me", NULL };
static const char* const find_phrases[] = { "search for ", "look for ", "find ", "search ",
                                            "where is ", NULL };

static bool is_filler(const char* word) {
    for (int i = 0; filler[i]; i++) {
        if (strcmp(word, filler[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* Copy the next word of 'text' (letters, digits, '.', '_', '-'), keeping
   the original case, from 'start' (a position in the lowercase copy). */
static const char* copy_word(const char* original, const char* lowered, const char* start,
                             char* out, int max) {
    while (*start == ' ' || *start == '"' || *start == '\'') {
        start++;
    }
    const char* from = original + (start - lowered);
    int n = 0;
    while (start[n] && (is_letter(start[n]) || start[n] == '.' || start[n] == '_' ||
                        start[n] == '-') && n < max - 1) {
        out[n] = from[n];
        n++;
    }
    out[n] = '\0';
    while (n > 0 && out[n - 1] == '.') {    /* "find notes." */
        out[--n] = '\0';
    }
    return start + n;
}

static bool route_find(const char* message, const char* text, tool_call_t* call) {
    for (int i = 0; find_phrases[i]; i++) {
        const char* at = find_text(text, find_phrases[i]);
        if (!at) {
            continue;
        }
        const char* p = at + strlen(find_phrases[i]);
        char lowered[TOOL_VALUE_MAX];
        do {
            p = copy_word(message, text, p, call->value, TOOL_VALUE_MAX);
            to_lower(call->value, lowered, sizeof(lowered));
        } while (call->value[0] && is_filler(lowered));
        if (call->value[0]) {
            call->kind = TOOL_FIND;
            return true;
        }
    }
    return false;
}

static bool route_open(const char* message, const char* text, tool_call_t* call) {
    const char* at = find_text(text, "open ");
    if (!at) {
        at = find_text(text, "edit ");
    }
    for (const char* p = at ? at + 5 : NULL; p && *p; ) {
        p = copy_word(message, text, p, call->value, TOOL_VALUE_MAX);
        if (find_text(call->value, ".")) {
            call->kind = TOOL_OPEN;
            return true;
        }
        if (!call->value[0]) {
            p++;
        }
    }
    return false;
}

static void add_slot(tool_call_t* call, const char* key, const char* value) {
    if (call->slot_count < TOOL_SLOTS_MAX && value[0]) {
        tool_slot_t* slot = &call->slots[call->slot_count++];
        strcpy(slot->key, key);
        strcpy(slot->value, value);
    }
}

/* The rest of the message after 'phrase', as typed: "says Hi there!". */
static void copy_rest(const char* message, const char* text, const char* phrase, char* out) {
    out[0] = '\0';
    const char* at = find_text(text, phrase);
    if (!at) {
        return;
    }
    const char* from = message + (at - text) + strlen(phrase);
    int n = 0;
    while (from[n] && n < TOOL_VALUE_MAX - 1) {
        out[n] = from[n];
        n++;
    }
    out[n] = '\0';
}

static const char* template_in(const char* text) {
    static const char* const names[] = { "counter", "clock", "box", "button", NULL };
    for (int i = 0; names[i]; i++) {
        if (has_word(text, names[i])) {
            return names[i];
        }
    }
    bool says = has_word(text, "says") || has_word(text, "saying");
    bool window = has_word(text, "window") || has_word(text, "app") || has_word(text, "message");
    return (says && window) ? "message" : NULL;
}

static bool route_make_app(const char* message, const char* text, tool_call_t* call) {
    static const char* const colors[] = { "red", "green", "blue", "yellow", "orange", "purple",
                                          "white", "black", "pink", "gray", "cyan", NULL };
    static const char* const sizes[] = { "small", "medium", "large", NULL };
    /* "show me a clock" is a question about the Clock app, so the other
       templates need a verb like "make"; "a message that says ..." doesn't. */
    const char* name = template_in(text);
    if (!name || (strcmp(name, "message") != 0 && !has_any_word(text, make_words))) {
        return false;
    }
    call->kind = TOOL_MAKE_APP;
    strcpy(call->value, name);

    char value[TOOL_VALUE_MAX];
    const char* at = find_text(text, "called ");
    at = at ? at : find_text(text, "named ");
    if (at) {
        copy_word(message, text, at + (at[0] == 'c' ? 7 : 6), value, TOOL_VALUE_MAX);
        add_slot(call, "title", value);
    }
    copy_rest(message, text, "says ", value);
    add_slot(call, "text", value);
    for (int i = 0; colors[i]; i++) {
        if (has_word(text, colors[i])) {
            add_slot(call, "color", colors[i]);
        }
    }
    for (int i = 0; sizes[i]; i++) {
        if (has_word(text, sizes[i])) {
            add_slot(call, "size", sizes[i]);
        }
    }
    return true;
}

/* Requests about the system itself: disk, memory, time, the file list. */
static tool_kind_t route_simple(const char* text) {
    static const char* const list[] = { "list", "show", "what", "which", NULL };

    if (has_word(text, "files") && has_any_word(text, list) && !find_text(text, "how many")) {
        return TOOL_FILES;
    }
    if (has_any_word(text, disk_words)) {
        return TOOL_DISK;
    }
    if (has_any_word(text, memory_words)) {
        return TOOL_MEMORY;
    }
    if (has_any_word(text, time_words) || find_text(text, "how long")) {
        return TOOL_TIME;
    }
    return TOOL_NONE;
}

bool tool_route(const char* message, tool_call_t* call) {
    char text[MESSAGE_MAX];
    to_lower(message, text, sizeof(text));
    memset(call, 0, sizeof(tool_call_t));

    for (int i = 0; how_to[i]; i++) {
        if (find_text(text, how_to[i])) {
            return false;
        }
    }
    if (route_make_app(message, text, call) || route_open(message, text, call) ||
        route_find(message, text, call)) {
        return true;
    }
    memset(call, 0, sizeof(tool_call_t));
    call->kind = route_simple(text);
    return call->kind != TOOL_NONE;
}
