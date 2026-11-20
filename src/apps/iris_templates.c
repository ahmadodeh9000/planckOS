/*
 * iris_templates.c - small assembly apps Iris can make from templates.
 *
 * Asked to "make a red counter called Taps", Iris answers with a tag,
 * [make-app counter title="Taps" color=red], and never with code: a model
 * this small can't write working assembly, and code it wrote could do
 * anything. Instead each template below is a complete, tested program
 * with {SLOTS}. template_fill() checks every value first:
 *
 *   - the template and the color must be ones listed here,
 *   - titles and texts must be short printable ASCII,
 *   - a '"' in a text becomes the byte 34 outside the quotes, so no value
 *     can end a string early and turn the rest into code,
 *
 * and only then pastes the values in. The result goes through the normal
 * assembler (iris_tools.c), and you decide whether to run it.
 *
 * How to add a template: write the program with {TITLE}, {COLOR}, ... in
 * it (the slots are listed at fill_values), add it to templates[], and
 * check it with `make templates-test`, which assembles every template
 * with every color and compares the bytes with NASM.
 */
#include "apps/iris_templates.h"
#include "lib/string.h"

#define VALUE_MAX       160         /* a filled-in slot, e.g. a quoted text */
#define BUTTON_TEXT_MAX 20          /* what fits on the button */
#define QUOTE_BYTE      "34"        /* '"' as a number, outside the quotes */
#define CHAR_WIDTH      8           /* the font, for centering text */

/* ---- the programs -------------------------------------------------- */

#define HEADER \
    "; {NAME}.ASM - made by Iris from the \"{TEMPLATE}\" template.\n" \
    "; Build and run it in a Terminal:  asm {NAME}.ASM {NAME}.ELF  then  exec {NAME}.ELF\n" \
    "\n" \
    "SYS_WIN_CREATE     equ 6\n" \
    "SYS_WIN_ON         equ 7\n" \
    "SYS_WIN_RECT       equ 8\n" \
    "SYS_WIN_TEXT       equ 9\n" \
    "SYS_WIN_INVALIDATE equ 10\n" \
    "SYS_TIME           equ 13\n" \
    "WIN_EVENT_PAINT    equ 0\n" \
    "WIN_EVENT_MOUSE    equ 2\n" \
    "WIN_EVENT_TIMER    equ 3\n" \
    "COLOR              equ {COLOR}      ; {COLOR_NAME}\n" \
    "INK                equ {INK}      ; text that shows up on COLOR\n" \
    "\n"

/* Open the window and register paint; every template starts like this. */
#define OPEN_WINDOW \
    "_start:\n" \
    "    mov eax, SYS_WIN_CREATE\n" \
    "    mov ebx, title\n" \
    "    mov ecx, {WIDTH}\n" \
    "    mov edx, {HEIGHT}\n" \
    "    int 0x80\n" \
    "    mov [win], eax              ; the window id, or 0\n" \
    "    mov eax, SYS_WIN_ON\n" \
    "    mov ebx, [win]\n" \
    "    mov ecx, WIN_EVENT_PAINT\n" \
    "    mov edx, paint\n" \
    "    int 0x80\n"

#define ON_EVENT(event, function) \
    "    mov eax, SYS_WIN_ON\n" \
    "    mov ebx, [win]\n" \
    "    mov ecx, " event "\n" \
    "    mov edx, " function "\n" \
    "    int 0x80\n"

#define RETURN_ZERO \
    "    xor eax, eax                ; exit code 0; the window stays open\n" \
    "    ret\n" \
    "\n"

#define INVALIDATE_AND_RETURN \
    "    mov eax, SYS_WIN_INVALIDATE\n" \
    "    mov ebx, [esp + 4]\n" \
    "    int 0x80\n" \
    "    ret\n" \
    "\n"

static const char MESSAGE_SOURCE[] = HEADER OPEN_WINDOW RETURN_ZERO
    "; paint(id): the message.\n"
    "paint:\n"
    "    mov eax, SYS_WIN_TEXT\n"
    "    mov ebx, [esp + 4]\n"
    "    mov ecx, 0x000C0012         ; x = 12, y = 18\n"
    "    mov edx, text\n"
    "    mov esi, COLOR\n"
    "    int 0x80\n"
    "    ret\n"
    "\n"
    "title: db {TITLE}, 0\n"
    "text:  db {TEXT}, 0\n"
    "win:   dd 0\n";

static const char BOX_SOURCE[] = HEADER OPEN_WINDOW RETURN_ZERO
    "; paint(id): one box, 10 pixels in from the edges.\n"
    "paint:\n"
    "    mov eax, SYS_WIN_RECT\n"
    "    mov ebx, [esp + 4]\n"
    "    mov ecx, 0x000A000A         ; x = 10, y = 10\n"
    "    mov edx, {BOX_SIZE}         ; width | height\n"
    "    mov esi, COLOR\n"
    "    int 0x80\n"
    "    ret\n"
    "\n"
    "title: db {TITLE}, 0\n"
    "win:   dd 0\n";

static const char COUNTER_SOURCE[] = HEADER OPEN_WINDOW
    ON_EVENT("WIN_EVENT_MOUSE", "click") RETURN_ZERO
    "; click(id, x, y): count, then ask for a repaint.\n"
    "click:\n"
    "    mov eax, [count]\n"
    "    inc eax\n"
    "    mov [count], eax\n"
    INVALIDATE_AND_RETURN
    "; paint(id): a box to click, and the count under it.\n"
    "paint:\n"
    "    mov eax, SYS_WIN_RECT\n"
    "    mov ebx, [esp + 4]\n"
    "    mov ecx, 0x000A000A         ; x = 10, y = 10\n"
    "    mov edx, 0x00C8002C         ; w = 200, h = 44\n"
    "    mov esi, COLOR\n"
    "    int 0x80\n"
    "    mov eax, SYS_WIN_TEXT\n"
    "    mov ecx, 0x0014001A         ; x = 20, y = 26: on the box\n"
    "    mov edx, hint\n"
    "    mov esi, INK\n"
    "    int 0x80\n"
    "    mov eax, SYS_WIN_TEXT\n"
    "    mov ecx, 0x000A0040         ; x = 10, y = 64\n"
    "    mov edx, label\n"
    "    mov esi, 0x00000000\n"
    "    int 0x80\n"
    "    call count_to_text          ; edi = the count as text\n"
    "    mov eax, SYS_WIN_TEXT\n"
    "    mov ecx, 0x004A0040         ; x = 74, right after the label\n"
    "    mov edx, edi\n"
    "    int 0x80\n"
    "    ret\n"
    "\n"
    "; count_to_text: [count] in decimal; returns edi = its first digit.\n"
    "; Digits come out last first (count % 10), so they're written backwards.\n"
    "count_to_text:\n"
    "    mov eax, [count]\n"
    "    mov edi, digits_end\n"
    "    mov ecx, 10\n"
    "next_digit:\n"
    "    xor edx, edx\n"
    "    div ecx                     ; eax = eax / 10, edx = the last digit\n"
    "    add edx, '0'\n"
    "    dec edi\n"
    "    mov [edi], dl\n"
    "    test eax, eax\n"
    "    jne next_digit\n"
    "    ret\n"
    "\n"
    "title:      db {TITLE}, 0\n"
    "hint:       db \"click me!\", 0\n"
    "label:      db \"clicks: \", 0\n"
    "digits:     times 10 db 0\n"
    "digits_end: db 0\n"
    "count:      dd 0\n"
    "win:        dd 0\n";

static const char CLOCK_SOURCE[] = HEADER OPEN_WINDOW
    ON_EVENT("WIN_EVENT_TIMER", "tick") RETURN_ZERO
    "; tick(id): once a second, repaint (paint reads the time).\n"
    "tick:\n"
    INVALIDATE_AND_RETURN
    "; paint(id): a panel with HH:MM:SS on it.\n"
    "paint:\n"
    "    mov eax, SYS_TIME\n"
    "    int 0x80                    ; eax = hour << 16 | minute << 8 | second\n"
    "    mov ebx, eax\n"
    "    shr eax, 16                 ; the hour\n"
    "    mov edi, time_text\n"
    "    call two_digits\n"
    "    mov eax, ebx\n"
    "    shr eax, 8\n"
    "    and eax, 0xFF               ; the minute\n"
    "    mov edi, time_text\n"
    "    add edi, 3\n"
    "    call two_digits\n"
    "    mov eax, ebx\n"
    "    and eax, 0xFF               ; the second\n"
    "    mov edi, time_text\n"
    "    add edi, 6\n"
    "    call two_digits\n"
    "    mov eax, SYS_WIN_RECT\n"
    "    mov ebx, [esp + 4]\n"
    "    mov ecx, 0x000A000A         ; x = 10, y = 10\n"
    "    mov edx, 0x00B40028         ; w = 180, h = 40\n"
    "    mov esi, COLOR\n"
    "    int 0x80\n"
    "    mov eax, SYS_WIN_TEXT\n"
    "    mov ecx, 0x00440016         ; x = 68, y = 22: centered\n"
    "    mov edx, time_text\n"
    "    mov esi, INK\n"
    "    int 0x80\n"
    "    ret\n"
    "\n"
    "; two_digits: write eax (0-99) as two digits at edi.\n"
    "two_digits:\n"
    "    mov ecx, 10\n"
    "    xor edx, edx\n"
    "    div ecx                     ; eax = tens, edx = ones\n"
    "    add eax, '0'\n"
    "    mov [edi], al\n"
    "    add edx, '0'\n"
    "    mov [edi + 1], dl\n"
    "    ret\n"
    "\n"
    "title:     db {TITLE}, 0\n"
    "time_text: db \"00:00:00\", 0\n"
    "win:       dd 0\n";

static const char BUTTON_SOURCE[] = HEADER OPEN_WINDOW
    ON_EVENT("WIN_EVENT_MOUSE", "click") RETURN_ZERO
    "; click(id, x, y): switch between COLOR and gray, then repaint.\n"
    "click:\n"
    "    mov eax, [pressed]\n"
    "    xor eax, 1\n"
    "    mov [pressed], eax\n"
    INVALIDATE_AND_RETURN
    "; paint(id): the button in its current color, with its label.\n"
    "paint:\n"
    "    mov esi, COLOR\n"
    "    mov eax, [pressed]\n"
    "    cmp eax, 0\n"
    "    je draw_button\n"
    "    mov esi, 0x00888A85         ; gray\n"
    "draw_button:\n"
    "    mov eax, SYS_WIN_RECT\n"
    "    mov ebx, [esp + 4]\n"
    "    mov ecx, 0x00140014         ; x = 20, y = 20\n"
    "    mov edx, 0x00B40032         ; w = 180, h = 50\n"
    "    int 0x80\n"
    "    mov eax, SYS_WIN_TEXT\n"
    "    mov ecx, {TEXT_POSITION}         ; centered on the button\n"
    "    mov edx, text\n"
    "    mov esi, INK\n"
    "    int 0x80\n"
    "    ret\n"
    "\n"
    "title:   db {TITLE}, 0\n"
    "text:    db {TEXT}, 0\n"
    "pressed: dd 0\n"
    "win:     dd 0\n";

typedef struct {
    const char* name;
    const char* source;
    const char* slots;              /* the slots you may set, besides title */
    const char* default_title;
    const char* default_text;
    const char* default_color;
} app_template_t;

static const app_template_t templates[] = {
    { "message", MESSAGE_SOURCE, "text color", "Message", "Hello from Iris!", "black" },
    { "box",     BOX_SOURCE,     "color size", "Box",     NULL,               "blue" },
    { "counter", COUNTER_SOURCE, "color",      "Counter", NULL,               "blue" },
    { "clock",   CLOCK_SOURCE,   "color",      "Clock",   NULL,               "blue" },
    { "button",  BUTTON_SOURCE,  "text color", "Button",  "Press me",         "green" },
};

#define TEMPLATE_COUNT ((int)(sizeof(templates) / sizeof(templates[0])))

typedef struct {
    const char* name;
    uint32_t rgb;
    uint32_t ink;                   /* readable text color on top of it */
} named_color_t;

static const named_color_t colors[] = {
    { "red",    0x00CC0000, 0x00FFFFFF }, { "green",  0x004E9A06, 0x00FFFFFF },
    { "blue",   0x003465A4, 0x00FFFFFF }, { "yellow", 0x00EDD400, 0x00000000 },
    { "orange", 0x00F57900, 0x00000000 }, { "purple", 0x0075507B, 0x00FFFFFF },
    { "white",  0x00FFFFFF, 0x00000000 }, { "black",  0x00000000, 0x00FFFFFF },
    { "pink",   0x00F4A6C0, 0x00000000 }, { "gray",   0x00888A85, 0x00000000 },
    { "cyan",   0x0000B4C8, 0x00000000 },
};

#define COLOR_COUNT ((int)(sizeof(colors) / sizeof(colors[0])))

typedef struct {
    const char* name;
    int width;
    int height;
} box_size_t;

static const box_size_t sizes[] = {
    { "small", 120, 80 }, { "medium", 200, 140 }, { "large", 320, 220 },
};

#define SIZE_COUNT      ((int)(sizeof(sizes) / sizeof(sizes[0])))
#define BOX_MARGIN      10      /* the box's distance from the window edge */
#define MESSAGE_MIN_W   160
#define MESSAGE_H       52
#define FIXED_W         220     /* counter, clock and button windows */
#define COUNTER_H       90
#define CLOCK_H         60
#define BUTTON_H        90
#define BUTTON_X        20      /* the button rectangle in the button template */
#define BUTTON_Y        20
#define BUTTON_W        180
#define BUTTON_H_INNER  50
#define FONT_H          16

const char* template_name_at(int index) {
    return (index >= 0 && index < TEMPLATE_COUNT) ? templates[index].name : NULL;
}

const char* template_color_at(int index) {
    return (index >= 0 && index < COLOR_COUNT) ? colors[index].name : NULL;
}

/* ---- checking the values ------------------------------------------- */

static const app_template_t* find_template(const char* name) {
    for (int i = 0; name && i < TEMPLATE_COUNT; i++) {
        if (strcmp(name, templates[i].name) == 0) {
            return &templates[i];
        }
    }
    return NULL;
}

static const named_color_t* find_color(const char* name) {
    for (int i = 0; i < COLOR_COUNT; i++) {
        if (strcmp(name, colors[i].name) == 0) {
            return &colors[i];
        }
    }
    return NULL;
}

static const box_size_t* find_size(const char* name) {
    for (int i = 0; i < SIZE_COUNT; i++) {
        if (strcmp(name, sizes[i].name) == 0) {
            return &sizes[i];
        }
    }
    return NULL;
}

/* Is 'slot' one of the words in the template's slot list? */
static bool has_slot(const app_template_t* t, const char* slot) {
    int n = strlen(slot);
    for (const char* s = t->slots; *s; s++) {
        if ((s == t->slots || s[-1] == ' ') && strncmp(s, slot, n) == 0 &&
            (s[n] == ' ' || s[n] == '\0')) {
            return true;
        }
    }
    return false;
}

/* 1 to 'max' printable ASCII characters. */
static bool printable(const char* text, int max) {
    int n = 0;
    for (; text[n]; n++) {
        if (text[n] < ' ' || text[n] > '~' || n >= max) {
            return false;
        }
    }
    return n > 0;
}

/* ---- filling in --------------------------------------------------- */

static void append(char* out, const char* text, int max) {
    int length = strlen(out);
    while (*text && length < max - 1) {
        out[length++] = *text++;
    }
    out[length] = '\0';
}

/* 'text' as a db operand: "say ", 34, "hi", 34. A '"' can't appear inside
   a string, so it is written as its byte value between two strings. */
static void quote_text(const char* text, char* out) {
    out[0] = '\0';
    bool open = false;
    bool first = true;
    for (; *text; text++) {
        if (*text == '"') {
            append(out, open ? "\", " QUOTE_BYTE : (first ? QUOTE_BYTE : ", " QUOTE_BYTE), VALUE_MAX);
            open = false;
        } else {
            if (!open) {
                append(out, first ? "\"" : ", \"", VALUE_MAX);
                open = true;
            }
            char c[2] = { *text, '\0' };
            append(out, c, VALUE_MAX);
        }
        first = false;
    }
    if (open) {
        append(out, "\"", VALUE_MAX);
    }
}

/* "0x00CC0000": always 8 digits, like the hand-written programs. */
static void hex_value(uint32_t value, char* out) {
    static const char digits[] = "0123456789ABCDEF";
    out[0] = '0';
    out[1] = 'x';
    for (int i = 0; i < 8; i++) {
        out[2 + i] = digits[(value >> (28 - 4 * i)) & 0xF];
    }
    out[10] = '\0';
}

static void decimal_value(uint32_t value, char* out) {
    char reversed[12];
    int n = 0;
    do {
        reversed[n++] = '0' + value % 10;
        value /= 10;
    } while (value);
    for (int i = 0; i < n; i++) {
        out[i] = reversed[n - 1 - i];
    }
    out[n] = '\0';
}

/* x|y (or width|height) packed the way the window calls expect. */
static void packed_value(int high, int low, char* out) {
    hex_value(((uint32_t)high << 16) | (uint32_t)low, out);
}

typedef struct {
    const char* key;
    char value[VALUE_MAX];
} slot_value_t;

enum { V_NAME, V_TEMPLATE, V_TITLE, V_TEXT, V_COLOR, V_COLOR_NAME, V_INK, V_WIDTH, V_HEIGHT,
       V_BOX_SIZE, V_TEXT_POSITION, V_COUNT };

/* Window size and the computed slots, per template. */
static void layout(const app_template_t* t, const char* text, const box_size_t* size,
                   slot_value_t* v) {
    int width = FIXED_W, height = COUNTER_H;
    int text_width = text ? (int)strlen(text) * CHAR_WIDTH : 0;

    if (t->source == MESSAGE_SOURCE) {
        width = text_width + 3 * BOX_MARGIN > MESSAGE_MIN_W ? text_width + 3 * BOX_MARGIN : MESSAGE_MIN_W;
        height = MESSAGE_H;
    } else if (t->source == BOX_SOURCE) {
        width = size->width;
        height = size->height;
    } else if (t->source == CLOCK_SOURCE) {
        height = CLOCK_H;
    } else if (t->source == BUTTON_SOURCE) {
        height = BUTTON_H;
    }
    decimal_value(width, v[V_WIDTH].value);
    decimal_value(height, v[V_HEIGHT].value);
    packed_value(size->width - 2 * BOX_MARGIN, size->height - 2 * BOX_MARGIN, v[V_BOX_SIZE].value);
    packed_value(BUTTON_X + (BUTTON_W - text_width) / 2, BUTTON_Y + (BUTTON_H_INNER - FONT_H) / 2,
                 v[V_TEXT_POSITION].value);
}

/* Copy 'source', replacing each {KEY} with its value. */
static bool substitute(const char* source, const slot_value_t* v, char* out, uint32_t capacity) {
    uint32_t n = 0;
    while (*source) {
        const char* value = NULL;
        int key_length = 0;
        for (int i = 0; *source == '{' && i < V_COUNT; i++) {
            key_length = strlen(v[i].key);
            if (strncmp(source + 1, v[i].key, key_length) == 0 && source[key_length + 1] == '}') {
                value = v[i].value;
                break;
            }
        }
        const char* piece = value ? value : source;
        uint32_t length = value ? strlen(value) : 1;
        if (n + length >= capacity) {
            return false;
        }
        memcpy(out + n, piece, length);
        n += length;
        source += value ? key_length + 2 : 1;
    }
    out[n] = '\0';
    return true;
}

/* Check the request; on success, fill 'v' with every slot's value. */
static const char* check_request(const template_request_t* r, const app_template_t* t,
                                 slot_value_t* v) {
    const char* title = r->title ? r->title : t->default_title;
    const char* text = r->text ? r->text : t->default_text;
    const named_color_t* color = find_color(r->color ? r->color : t->default_color);
    const box_size_t* size = find_size(r->size ? r->size : "medium");

    if ((r->text && !has_slot(t, "text")) || (r->color && !has_slot(t, "color")) ||
        (r->size && !has_slot(t, "size"))) {
        return "that template doesn't have that option";
    }
    if (!printable(title, TEMPLATE_TITLE_MAX)) {
        return "the title must be 1 to 20 normal characters";
    }
    int text_max = t->source == BUTTON_SOURCE ? BUTTON_TEXT_MAX : TEMPLATE_TEXT_MAX;
    if (text && !printable(text, text_max)) {
        return "the text must be 1 to 32 normal characters (20 on a button)";
    }
    if (!color) {
        return "i only know the colors red, green, blue, yellow, orange, purple, white, "
               "black, pink, gray and cyan";
    }
    if (!size) {
        return "the size can be small, medium or large";
    }
    quote_text(title, v[V_TITLE].value);
    quote_text(text ? text : "", v[V_TEXT].value);
    hex_value(color->rgb, v[V_COLOR].value);
    strcpy(v[V_COLOR_NAME].value, color->name);
    hex_value(color->ink, v[V_INK].value);
    layout(t, text, size, v);
    return NULL;
}

const char* template_fill(const template_request_t* request, char* source, uint32_t capacity) {
    static const char* const keys[V_COUNT] = {
        "NAME", "TEMPLATE", "TITLE", "TEXT", "COLOR", "COLOR_NAME", "INK", "WIDTH", "HEIGHT",
        "BOX_SIZE", "TEXT_POSITION",
    };
    const app_template_t* t = find_template(request->template_name);
    if (!t) {
        return "i only have the templates message, box, counter, clock and button";
    }
    slot_value_t values[V_COUNT];
    for (int i = 0; i < V_COUNT; i++) {
        values[i].key = keys[i];
        values[i].value[0] = '\0';
    }
    const char* problem = check_request(request, t, values);
    if (problem) {
        return problem;
    }
    strcpy(values[V_NAME].value, request->name);
    strcpy(values[V_TEMPLATE].value, t->name);
    return substitute(t->source, values, source, capacity) ? NULL : "the program got too long";
}

void template_base_name(const template_request_t* request, char* base) {
    const char* from = request->title ? request->title : request->template_name;
    int n = 0;
    for (; *from && n < 8; from++) {
        char c = *from;
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            base[n++] = c;
        }
    }
    base[n] = '\0';
    if (n == 0) {
        strcpy(base, "APP");
    }
}
