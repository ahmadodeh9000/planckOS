/*
 * builtins.c - Higgson's built-in functions and values (see builtins.h).
 *
 * GUI apps map straight onto planckOS's program window calls
 * (docs/PROGRAMS_GUI.md): window() is SYS_WIN_CREATE, on_paint() and
 * friends are SYS_WIN_ON with the address of a func, rect() is
 * SYS_WIN_RECT, and so on. A program has one window, whose id is kept in
 * _hg_win.
 *
 * The kernel calls an event func with the C calling convention:
 * paint(id), mouse(id, x, y), key(id, key), timer(id). A func given to
 * on_click or on_key gets a small entry in front of it that copies the
 * event's values into mouse_x / mouse_y or key, then jumps to the func's
 * body, whose 'ret' goes straight back to the kernel. Each event has its
 * own entry because the same func may be used for several events, or
 * called directly.
 */
#include "higgson/builtins.h"
#include "higgson/hg.h"
#include "higgson/util.h"

#define HALF_SHIFT      16          /* two 16-bit values in one register */
#define LOW_HALF_MASK   65535       /* 0xFFFF */
#define BYTE_MASK       255
#define DIGITS_SIZE     11          /* "-2147483648" */
#define ITOA_LABELS     6           /* labels _hg_itoa defines */

const char* const hg_builtin_constants[] = {
    "SYS_PRINT          equ 0",
    "SYS_PRINT_NUMBER   equ 5",
    "SYS_WIN_CREATE     equ 6",
    "SYS_WIN_ON         equ 7",
    "SYS_WIN_RECT       equ 8",
    "SYS_WIN_TEXT       equ 9",
    "SYS_WIN_INVALIDATE equ 10",
    "SYS_WIN_CLOSE      equ 11",
    "SYS_WIN_LINE       equ 12",
    "SYS_TIME           equ 13",
    "WIN_EVENT_PAINT    equ 0",
    "WIN_EVENT_KEY      equ 1",
    "WIN_EVENT_MOUSE    equ 2",
    "WIN_EVENT_TIMER    equ 3",
};

const uint32_t hg_builtin_constant_count =
    sizeof(hg_builtin_constants) / sizeof(hg_builtin_constants[0]);

/* planckOS's own palette (the same colors Iris's apps use). */
static const struct { const char* name; uint32_t rgb; } hg_colors[] = {
    { "BLACK",   0x000000 }, { "WHITE",  0xFFFFFF }, { "RED",     0xCC0000 },
    { "GREEN",   0x4E9A06 }, { "BLUE",   0x3465A4 }, { "YELLOW",  0xEDD400 },
    { "CYAN",    0x00B4C8 }, { "MAGENTA", 0xC0399C }, { "GRAY",   0x888A85 },
    { "ORANGE",  0xF57900 },
};

#define COLOR_COUNT (sizeof(hg_colors) / sizeof(hg_colors[0]))

/* Values a program can read but not change. */
static const struct { const char* name; const char* label; } hg_event_values[] = {
    { "mouse_x", "_hg_mouse_x" }, { "mouse_y", "_hg_mouse_y" }, { "key", "_hg_key" },
};

#define EVENT_VALUE_COUNT (sizeof(hg_event_values) / sizeof(hg_event_values[0]))

/* hour(), minute(), second(): SYS_TIME gives (h << 16) | (m << 8) | s. */
static const struct { const char* name; int shift; } hg_clock[] = {
    { "hour", 16 }, { "minute", 8 }, { "second", 0 },
};

#define CLOCK_COUNT (sizeof(hg_clock) / sizeof(hg_clock[0]))

static void hg_window(hg_parser_t* parser);
static void hg_on_event(hg_parser_t* parser);
static void hg_draw(hg_parser_t* parser);
static void hg_window_call(hg_parser_t* parser);

/* Built-in statements: name, how to parse it, and for the ones with a
   syscall of their own, which. */
static const struct {
    const char* name;
    void (*parse)(hg_parser_t* parser);
    const char* call;
} hg_statements[] = {
    { "window",   hg_window,      NULL },
    { "on_paint", hg_on_event,    "WIN_EVENT_PAINT" },
    { "on_click", hg_on_event,    "WIN_EVENT_MOUSE" },
    { "on_key",   hg_on_event,    "WIN_EVENT_KEY" },
    { "on_timer", hg_on_event,    "WIN_EVENT_TIMER" },
    { "rect",     hg_draw,        "SYS_WIN_RECT" },
    { "line",     hg_draw,        "SYS_WIN_LINE" },
    { "text",     hg_draw,        "SYS_WIN_TEXT" },
    { "number",   hg_draw,        "SYS_WIN_TEXT" },
    { "repaint",  hg_window_call, "SYS_WIN_INVALIDATE" },
    { "close",    hg_window_call, "SYS_WIN_CLOSE" },
};

#define STATEMENT_COUNT (sizeof(hg_statements) / sizeof(hg_statements[0]))

static bool hg_is(const char* a, const char* b) {
    return hg_util_strcmp(a, b) == 0;
}

static int hg_find_statement(const char* name) {
    for (uint32_t i = 0; i < STATEMENT_COUNT; i++) {
        if (hg_is(hg_statements[i].name, name)) {
            return (int)i;
        }
    }
    return -1;
}

bool hg_is_builtin(const char* name) {
    if (hg_find_statement(name) >= 0) {
        return true;
    }
    for (uint32_t i = 0; i < COLOR_COUNT; i++) {
        if (hg_is(hg_colors[i].name, name)) return true;
    }
    for (uint32_t i = 0; i < EVENT_VALUE_COUNT; i++) {
        if (hg_is(hg_event_values[i].name, name)) return true;
    }
    for (uint32_t i = 0; i < CLOCK_COUNT; i++) {
        if (hg_is(hg_clock[i].name, name)) return true;
    }
    return false;
}

/* The window id and the event values, added to the data on first use. */
static void hg_gui_data(hg_parser_t* parser) {
    if (parser->gui_data) {
        return;
    }
    parser->gui_data = true;
    hg_data_line(parser->emitter, "_hg_win: dd 0");
    hg_emitter_symbol(parser->emitter);
    for (uint32_t i = 0; i < EVENT_VALUE_COUNT; i++) {
        hg_data_line(parser->emitter, "%s: dd 0", hg_event_values[i].label);
        hg_emitter_symbol(parser->emitter);
    }
}

/* A string argument, put in the data; returns its label number. */
static int hg_string_argument(hg_parser_t* parser, const char* what) {
    if (!hg_check_token(parser, TOK_STRING)) {
        hg_error(parser->curr_token.line, "%s but found %s", what, hg_found(parser));
    }
    int label = hg_new_label(parser->emitter);
    hg_data_line(parser->emitter, "S%d: db \"%s\", 0", label, parser->curr_token.text);
    hg_next_token(parser);
    return label;
}

/* An expression argument, kept on the stack for later. */
static void hg_push_argument(hg_parser_t* parser) {
    hg_value(parser);
    hg_emit_line(parser->emitter, "    push eax");
    hg_match(parser, TOK_COMMA, ",");
}

/* ---- window() --------------------------------------------------------- */

/* window("title", width, height); opens the program's window. It must be
   in the main code itself, not in a func, if or while: so it runs exactly
   once, before anything that uses the window. If the kernel refuses (a
   size over 800 x 600, say), the program says so and ends with code 1. */
static void hg_window(hg_parser_t* parser) {
    hg_emitter_t* e = parser->emitter;
    int line = parser->curr_token.line;

    if (parser->window_line) {
        hg_error(line, "window() can only be called once (it already is on line %d)",
                 parser->window_line);
    }
    if (parser->func || parser->depth > 0) {
        hg_error(line, "window() must be in the main code, not inside a func, if or while");
    }
    hg_gui_data(parser);
    hg_next_token(parser);
    hg_match(parser, TOK_LPAREN, "(");
    int title = hg_string_argument(parser, "window() needs a title string first");
    hg_match(parser, TOK_COMMA, ",");
    hg_push_argument(parser);                   /* width */
    hg_value(parser);                           /* height */
    hg_match(parser, TOK_RPAREN, ")");
    hg_semicolon(parser);

    int message = hg_new_label(e);
    int opened = hg_new_label(e);
    hg_data_line(e, "S%d: db \"window(): planckOS couldn't open the window\", 10, 0", message);
    hg_emit_line(e, "    mov edx, eax");
    hg_emit_line(e, "    pop ecx");
    hg_emit_line(e, "    mov ebx, S%d", title);
    hg_emit_line(e, "    mov eax, SYS_WIN_CREATE");
    hg_emit_line(e, "    int 0x80");
    hg_emit_line(e, "    mov [_hg_win], eax");
    hg_emit_line(e, "    cmp eax, 0");
    hg_emit_line(e, "    jne L%d", opened);
    hg_emit_line(e, "    mov eax, SYS_PRINT");
    hg_emit_line(e, "    mov ebx, S%d", message);
    hg_emit_line(e, "    int 0x80");
    hg_emit_line(e, "    mov eax, 1");
    hg_emit_line(e, "    ret");
    hg_emit_line(e, "L%d:", opened);
    parser->window_line = line;
}

/* Everything but window() and the values needs the window to exist. In
   the main code, window() has to come first (further up), or the call
   would run before the window is open. A func runs later, on an event,
   so for a func it's enough that window() is somewhere: checked at the
   end, in hg_builtins_finish. */
static void hg_need_window(hg_parser_t* parser, const char* name, int line) {
    if (parser->window_line) {
        return;
    }
    if (!parser->func) {
        hg_error(line, "%s() needs a window: call window() first", name);
    }
    if (!parser->window_needed_line) {
        parser->window_needed_line = line;
        parser->window_needed_by = name;
    }
}

/* ---- on_paint(f) and the other events ------------------------------- */

/* The func's entry for this event, if it needs one (see the top). */
static const char* hg_event_entry(hg_parser_t* parser, const char* event, const char* func) {
    if (hg_is(event, "WIN_EVENT_MOUSE")) {
        hg_set_add(parser, &parser->click_handlers, func);
        return "_click_";
    }
    if (hg_is(event, "WIN_EVENT_KEY")) {
        hg_set_add(parser, &parser->key_handlers, func);
        return "_key_";
    }
    return "_f_";       /* paint and timer bring no values: the func itself */
}

static void hg_on_event(hg_parser_t* parser) {
    hg_emitter_t* e = parser->emitter;
    const char* name = parser->curr_token.text;
    const char* event = hg_statements[hg_find_statement(name)].call;
    int line = parser->curr_token.line;

    hg_need_window(parser, name, line);
    hg_next_token(parser);
    hg_match(parser, TOK_LPAREN, "(");
    if (!hg_check_token(parser, TOK_IDENT) || hg_is_builtin(parser->curr_token.text)) {
        hg_error(parser->curr_token.line, "%s() needs the name of a func but found %s",
                 name, hg_found(parser));
    }
    const char* func = parser->curr_token.text;
    if (hg_set_contains(&parser->symbols, func)) {
        hg_error(parser->curr_token.line, "'%s' is a variable, not a func", func);
    }
    hg_set_add(parser, &parser->funcs_called, func);
    hg_next_token(parser);
    hg_match(parser, TOK_RPAREN, ")");
    hg_semicolon(parser);

    hg_emit_line(e, "    mov eax, SYS_WIN_ON");
    hg_emit_line(e, "    mov ebx, [_hg_win]");
    hg_emit_line(e, "    mov ecx, %s", event);
    hg_emit_line(e, "    mov edx, %s%s", hg_event_entry(parser, event, func), func);
    hg_emit_line(e, "    int 0x80");
}

/* ---- drawing ---------------------------------------------------------- */

/* The kernel takes two 16-bit values packed in one register, as
   (first << 16) | second. Pops the second, then the first, into 'reg'. */
static void hg_emit_pop_pair(hg_emitter_t* e, const char* reg) {
    hg_emit_line(e, "    pop %s", reg);
    hg_emit_line(e, "    pop eax");
    hg_emit_line(e, "    shl eax, %d", HALF_SHIFT);
    hg_emit_line(e, "    and %s, %d", reg, LOW_HALF_MASK);
    hg_emit_line(e, "    or %s, eax", reg);
}

/* The third argument: a second x|y pair for rect and line, a string for
   text, a number turned into text for number. Leaves edx's value on the
   stack, or (rect, line) two more values. */
static void hg_draw_third(hg_parser_t* parser, const char* name) {
    hg_emitter_t* e = parser->emitter;

    if (hg_is(name, "text")) {
        int label = hg_string_argument(parser, "text() needs a string as its third argument (number() draws numbers)");
        hg_emit_line(e, "    push S%d", label);
        hg_match(parser, TOK_COMMA, ",");
    }
    else if (hg_is(name, "number")) {
        hg_value(parser);
        hg_emit_line(e, "    call _hg_itoa");
        hg_emit_line(e, "    push eax");
        hg_match(parser, TOK_COMMA, ",");
        parser->uses_number = true;
    }
    else {
        hg_push_argument(parser);
        hg_push_argument(parser);
    }
}

/* rect(x, y, w, h, color); line(x0, y0, x1, y1, color);
   text(x, y, "string", color); number(x, y, expression, color);
   Only inside a func: planckOS lets a window draw only while it's being
   painted (docs/PROGRAMS_GUI.md), and the main code runs before that. */
static void hg_draw(hg_parser_t* parser) {
    hg_emitter_t* e = parser->emitter;
    const char* name = parser->curr_token.text;
    int line = parser->curr_token.line;
    bool two_pairs = hg_is(name, "rect") || hg_is(name, "line");

    if (!parser->func) {
        hg_error(line, "%s() only works inside a func: a window can only draw while "
                       "it's painted", name);
    }
    hg_need_window(parser, name, line);
    hg_next_token(parser);
    hg_match(parser, TOK_LPAREN, "(");
    hg_push_argument(parser);
    hg_push_argument(parser);
    hg_draw_third(parser, name);
    hg_value(parser);                   /* color */
    hg_match(parser, TOK_RPAREN, ")");
    hg_semicolon(parser);

    hg_emit_line(e, "    mov esi, eax");
    if (two_pairs) {
        hg_emit_pop_pair(e, "edx");
    } else {
        hg_emit_line(e, "    pop edx");
    }
    hg_emit_pop_pair(e, "ecx");
    hg_emit_line(e, "    mov ebx, [_hg_win]");
    hg_emit_line(e, "    mov eax, %s", hg_statements[hg_find_statement(name)].call);
    hg_emit_line(e, "    int 0x80");
}

/* repaint(); and close(); */
static void hg_window_call(hg_parser_t* parser) {
    const char* name = parser->curr_token.text;

    hg_need_window(parser, name, parser->curr_token.line);
    hg_next_token(parser);
    hg_match(parser, TOK_LPAREN, "(");
    hg_match(parser, TOK_RPAREN, ")");
    hg_semicolon(parser);
    hg_emit_line(parser->emitter, "    mov eax, %s", hg_statements[hg_find_statement(name)].call);
    hg_emit_line(parser->emitter, "    mov ebx, [_hg_win]");
    hg_emit_line(parser->emitter, "    int 0x80");
}

bool hg_builtin_statement(hg_parser_t* parser) {
    if (!hg_check_token(parser, TOK_IDENT)) {
        return false;
    }
    int i = hg_find_statement(parser->curr_token.text);
    if (i < 0) {
        if (hg_is_builtin(parser->curr_token.text) && parser->peek_token.type == TOK_EQ) {
            hg_error(parser->curr_token.line, "'%s' is built in and can't be changed",
                     parser->curr_token.text);
        }
        if (hg_is_builtin(parser->curr_token.text)) {
            hg_error(parser->curr_token.line, "'%s' gives a value: use it in an expression",
                     parser->curr_token.text);
        }
        return false;
    }
    if (parser->peek_token.type != TOK_LPAREN) {
        hg_error(parser->curr_token.line, "expected '(' after '%s'", parser->curr_token.text);
    }
    hg_statements[i].parse(parser);
    return true;
}

/* ---- values ----------------------------------------------------------- */

/* hour(), minute() or second(). */
static void hg_clock_value(hg_parser_t* parser, int shift) {
    hg_emitter_t* e = parser->emitter;

    hg_next_token(parser);
    hg_match(parser, TOK_LPAREN, "(");
    hg_match(parser, TOK_RPAREN, ")");
    hg_emit_line(e, "    mov eax, SYS_TIME");
    hg_emit_line(e, "    int 0x80");
    if (shift) {
        hg_emit_line(e, "    shr eax, %d", shift);
    }
    hg_emit_line(e, "    and eax, %d", BYTE_MASK);
}

bool hg_builtin_value(hg_parser_t* parser) {
    const char* name = parser->curr_token.text;

    for (uint32_t i = 0; i < COLOR_COUNT; i++) {
        if (hg_is(hg_colors[i].name, name)) {
            hg_emit_line(parser->emitter, "    mov eax, %u          ; %s", hg_colors[i].rgb, name);
            hg_next_token(parser);
            return true;
        }
    }
    for (uint32_t i = 0; i < EVENT_VALUE_COUNT; i++) {
        if (hg_is(hg_event_values[i].name, name)) {
            hg_gui_data(parser);
            hg_emit_line(parser->emitter, "    mov eax, [%s]", hg_event_values[i].label);
            hg_next_token(parser);
            return true;
        }
    }
    for (uint32_t i = 0; i < CLOCK_COUNT; i++) {
        if (hg_is(hg_clock[i].name, name)) {
            hg_clock_value(parser, hg_clock[i].shift);
            return true;
        }
    }
    if (hg_find_statement(name) >= 0) {
        hg_error(parser->curr_token.line, "%s() gives no value: call it on its own line", name);
    }
    return false;
}

/* ---- after the program ---------------------------------------------- */

/* The entries in front of the funcs given to on_click and on_key: the
   kernel calls mouse(id, x, y) and key(id, key), so x is at [esp + 8],
   y (or the key) at [esp + 12] ... after the return address and the id. */
static void hg_emit_entries(hg_parser_t* parser) {
    hg_emitter_t* e = parser->emitter;

    for (int i = 0; i < parser->click_handlers.count; i++) {
        const char* func = parser->click_handlers.items[i];
        hg_emitter_symbol(e);
        hg_emit_line(e, "_click_%s:                ; on_click: mouse(id, x, y)", func);
        hg_emit_line(e, "    mov eax, [esp + 8]");
        hg_emit_line(e, "    mov [_hg_mouse_x], eax");
        hg_emit_line(e, "    mov eax, [esp + 12]");
        hg_emit_line(e, "    mov [_hg_mouse_y], eax");
        hg_emit_line(e, "    jmp _f_%s", func);
        hg_emit_line(e, "");
    }
    for (int i = 0; i < parser->key_handlers.count; i++) {
        const char* func = parser->key_handlers.items[i];
        hg_emitter_symbol(e);
        hg_emit_line(e, "_key_%s:                  ; on_key: key(id, key)", func);
        hg_emit_line(e, "    mov eax, [esp + 8]");
        hg_emit_line(e, "    mov [_hg_key], eax");
        hg_emit_line(e, "    jmp _f_%s", func);
        hg_emit_line(e, "");
    }
}

/* number() needs a number as text, and planckOS only draws text. This
   helper is added to the program only when number() is used. */
static void hg_emit_itoa(hg_emitter_t* e) {
    hg_emit_line(e, "; _hg_itoa: the number in eax as text, for number(). Divides by 10");
    hg_emit_line(e, "; again and again, writing each remainder as a digit from the end of");
    hg_emit_line(e, "; _hg_digits backwards, then a '-' if it was negative. Returns the");
    hg_emit_line(e, "; address of the first character in eax.");
    hg_emit_line(e, "_hg_itoa:");
    hg_emit_line(e, "    mov ecx, _hg_digits_end");
    hg_emit_line(e, "    mov ebx, eax              ; remember the sign");
    hg_emit_line(e, "    cmp eax, 0");
    hg_emit_line(e, "    jge _hg_itoa_digit");
    hg_emit_line(e, "    neg eax                   ; as unsigned, right even for -2147483648");
    hg_emit_line(e, "_hg_itoa_digit:");
    hg_emit_line(e, "    xor edx, edx");
    hg_emit_line(e, "    mov esi, 10");
    hg_emit_line(e, "    div esi                   ; eax = eax / 10, edx = the last digit");
    hg_emit_line(e, "    add edx, 48               ; '0'");
    hg_emit_line(e, "    dec ecx");
    hg_emit_line(e, "    mov [ecx], dl");
    hg_emit_line(e, "    cmp eax, 0");
    hg_emit_line(e, "    jne _hg_itoa_digit");
    hg_emit_line(e, "    cmp ebx, 0");
    hg_emit_line(e, "    jge _hg_itoa_done");
    hg_emit_line(e, "    dec ecx");
    hg_emit_line(e, "    mov byte [ecx], 45        ; '-'");
    hg_emit_line(e, "_hg_itoa_done:");
    hg_emit_line(e, "    mov eax, ecx");
    hg_emit_line(e, "    ret");
    hg_emit_line(e, "");
    hg_data_line(e, "_hg_digits: times %d db 0", DIGITS_SIZE);
    hg_data_line(e, "_hg_digits_end: db 0");
    for (int i = 0; i < ITOA_LABELS; i++) {
        hg_emitter_symbol(e);   /* the 4 labels above and the 2 in the data */
    }
}

void hg_builtins_finish(hg_parser_t* parser) {
    if (parser->window_needed_line && !parser->window_line) {
        hg_error(parser->window_needed_line, "%s() needs a window, but the program "
                 "never calls window()", parser->window_needed_by);
    }
    hg_emitter_section(parser->emitter, HG_SECTION_FUNCS);
    hg_emit_entries(parser);
    if (parser->uses_number) {
        hg_emit_itoa(parser->emitter);
    }
    hg_emitter_section(parser->emitter, HG_SECTION_MAIN);
}
