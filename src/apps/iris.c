/*
 * iris.c - the Iris app: a chat window with planckOS's assistant.
 *
 * The window shows the conversation (her messages next to her portrait,
 * yours on the right in blue) above an input box and a Send button.
 *
 * She writes one token per on_idle call (gui.c's idle work), so the rest
 * of the desktop stays responsive. The model is loaded on the first idle
 * call, after the window is on screen, and freed when it closes.
 */
#include "apps/iris.h"
#include "apps/iris_avatar.h"
#include "llm/chat.h"
#include "gui/wm.h"
#include "gui/widget.h"
#include "gui/theme.h"
#include "gfx/font.h"
#include "drivers/timer.h"
#include "mm/kmalloc.h"
#include "lib/string.h"

#define IRIS_WIDTH          560
#define IRIS_HEIGHT         440
#define PADDING             8
#define INPUT_HEIGHT        24
#define SEND_WIDTH          64
#define BUBBLE_PADDING      6
#define BUBBLE_GAP          10          /* between messages */
#define PORTRAIT_GAP        8           /* between her portrait and bubble */
#define USER_INDENT         96          /* your bubbles leave this much on the left */
#define MESSAGES_MAX        24
#define SCROLL_STEP         (2 * FONT_HEIGHT)
#define BLINK_PERIOD        120         /* ticks between blinks while thinking */
#define BLINK_LENGTH        15          /* ticks the eyes stay closed */
#define DOTS_PERIOD         40          /* ticks per "..." animation step */

#define COLOR_USER_BUBBLE   THEME_TITLE_ACTIVE
#define COLOR_IRIS_BUBBLE   THEME_FIELD
#define COLOR_BUBBLE_BORDER THEME_BUTTON_BORDER

typedef struct {
    bool from_iris;
    char text[CHAT_REPLY_MAX];
} iris_message_t;

typedef struct {
    chat_t* chat;
    bool load_tried;
    iris_message_t messages[MESSAGES_MAX];      /* oldest first */
    int message_count;
    int scroll;                                 /* pixels scrolled up */
    char input[CHAT_MESSAGE_MAX + 1];
    textbox_t input_box;
} iris_state;

/* ---- layout ------------------------------------------------------- */

static rect_t input_rect(void) {
    rect_t r = { PADDING, IRIS_HEIGHT - PADDING - INPUT_HEIGHT,
                 IRIS_WIDTH - 3 * PADDING - SEND_WIDTH, INPUT_HEIGHT };
    return r;
}

static rect_t send_rect(void) {
    rect_t r = { IRIS_WIDTH - PADDING - SEND_WIDTH, IRIS_HEIGHT - PADDING - INPUT_HEIGHT,
                 SEND_WIDTH, INPUT_HEIGHT };
    return r;
}

/* Text columns that fit in a bubble. */
static int bubble_columns(bool from_iris) {
    int left = from_iris ? PADDING + IRIS_PORTRAIT_SIZE + PORTRAIT_GAP : USER_INDENT;
    return (IRIS_WIDTH - left - PADDING - 2 * BUBBLE_PADDING) / FONT_WIDTH;
}

/* The next line of 'text' that fits in 'columns': its length, and in
   '*next' where the line after it starts. Breaks at spaces when it can. */
static int wrap_line(const char* text, int columns, const char** next) {
    int length = 0;
    int last_space = -1;

    while (text[length] && text[length] != '\n' && length < columns) {
        if (text[length] == ' ') {
            last_space = length;
        }
        length++;
    }
    if (text[length] && text[length] != '\n' && last_space > 0) {
        length = last_space;        /* don't cut a word in half */
    }
    *next = text + length;
    if (**next == ' ' || **next == '\n') {
        (*next)++;
    }
    return length;
}

/* Lines 'text' needs, and the longest one (in columns). */
static int count_lines(const char* text, int columns, int* widest) {
    int lines = 0;
    *widest = 0;
    while (*text) {
        int length = wrap_line(text, columns, &text);
        if (length > *widest) {
            *widest = length;
        }
        lines++;
    }
    return lines ? lines : 1;
}

/* ---- drawing ------------------------------------------------------ */

static void draw_wrapped(surface_t* s, int x, int y, const char* text, int columns,
                         uint32_t color) {
    char line[IRIS_WIDTH / FONT_WIDTH + 1];
    while (*text) {
        const char* start = text;
        int length = wrap_line(text, columns, &text);
        memcpy(line, start, length);
        line[length] = '\0';
        font_draw_text(s, x, y, line, color);
        y += FONT_HEIGHT;
    }
}

/* What a message shows: "..." (animated) while she hasn't written yet. */
static const char* message_text(const iris_message_t* m) {
    static const char* dots[] = { ".", "..", "..." };
    if (m->from_iris && m->text[0] == '\0') {
        return dots[(timer_ticks() / DOTS_PERIOD) % 3];
    }
    return m->text;
}

static int message_height(const iris_message_t* m) {
    int widest;
    int lines = count_lines(message_text(m), bubble_columns(m->from_iris), &widest);
    int height = lines * FONT_HEIGHT + 2 * BUBBLE_PADDING;
    return (m->from_iris && height < IRIS_PORTRAIT_SIZE) ? IRIS_PORTRAIT_SIZE : height;
}

static void draw_bubble(surface_t* s, rect_t r, uint32_t fill) {
    surface_fill_rect(s, r.x, r.y, r.w, r.h, fill);
    surface_draw_rect(s, r.x, r.y, r.w, r.h, COLOR_BUBBLE_BORDER);
}

/* Her portrait blinks now and then while she's working on the newest one. */
static const avatar_image_t* portrait(const iris_state* st, bool newest) {
    bool thinking = newest && st->chat && chat_busy(st->chat);
    if (thinking && timer_ticks() % BLINK_PERIOD < BLINK_LENGTH) {
        return &iris_portrait_blink;
    }
    return &iris_portrait;
}

static void draw_message(surface_t* s, const iris_state* st, int index, int y) {
    const iris_message_t* m = &st->messages[index];
    const char* text = message_text(m);
    int columns = bubble_columns(m->from_iris);
    int widest;
    int lines = count_lines(text, columns, &widest);
    int w = widest * FONT_WIDTH + 2 * BUBBLE_PADDING;
    int h = lines * FONT_HEIGHT + 2 * BUBBLE_PADDING;

    if (m->from_iris) {
        avatar_draw(s, PADDING, y, portrait(st, index == st->message_count - 1));
        rect_t bubble = { PADDING + IRIS_PORTRAIT_SIZE + PORTRAIT_GAP, y, w, h };
        draw_bubble(s, bubble, COLOR_IRIS_BUBBLE);
        draw_wrapped(s, bubble.x + BUBBLE_PADDING, y + BUBBLE_PADDING, text, columns, THEME_TEXT);
    } else {
        rect_t bubble = { IRIS_WIDTH - PADDING - w, y, w, h };
        draw_bubble(s, bubble, COLOR_USER_BUBBLE);
        draw_wrapped(s, bubble.x + BUBBLE_PADDING, y + BUBBLE_PADDING, text, columns,
                     THEME_TEXT_LIGHT);
    }
}

/* Newest at the bottom: walk back from the last message until the top. */
static void draw_messages(surface_t* s, const iris_state* st) {
    int bottom = input_rect().y - PADDING + st->scroll;

    for (int i = st->message_count - 1; i >= 0 && bottom > 0; i--) {
        int top = bottom - message_height(&st->messages[i]);
        draw_message(s, st, i, top);
        bottom = top - BUBBLE_GAP;
    }
}

static void iris_paint(window_t* win) {
    iris_state* st = win->data;
    surface_t* s = &win->surface;
    rect_t input = input_rect();

    draw_messages(s, st);
    /* The input row is drawn last, over any message that scrolled into it. */
    surface_fill_rect(s, 0, input.y - PADDING, IRIS_WIDTH, IRIS_HEIGHT - input.y + PADDING,
                      THEME_WINDOW_BG);
    widget_textbox(s, input, &st->input_box, true);
    widget_button(s, send_rect(), (st->chat && chat_busy(st->chat)) ? "..." : "Send");
}

/* ---- the conversation --------------------------------------------- */

static iris_message_t* add_message(iris_state* st, bool from_iris, const char* text) {
    if (st->message_count == MESSAGES_MAX) {
        memmove(&st->messages[0], &st->messages[1], (MESSAGES_MAX - 1) * sizeof(iris_message_t));
        st->message_count--;
    }
    iris_message_t* m = &st->messages[st->message_count++];
    m->from_iris = from_iris;
    strcpy(m->text, text);
    st->scroll = 0;
    return m;
}

static void iris_send(window_t* win) {
    iris_state* st = win->data;
    if (!st->chat || chat_busy(st->chat) || st->input[0] == '\0') {
        return;
    }
    add_message(st, false, st->input);
    if (chat_send(st->chat, st->input)) {
        add_message(st, true, "");          /* her reply grows in here */
        win->idle_work = true;
    } else {
        add_message(st, true, "sorry, that was too long for me. try a shorter message?");
    }
    st->input[0] = '\0';
}

/* The first idle call: load the model, now that the window is visible. */
static void load_model(iris_state* st) {
    llm_status status;
    st->load_tried = true;
    st->chat = chat_open(&status);
    if (st->chat) {
        add_message(st, true, "hi! i'm iris, planckOS's assistant. ask me anything about planckOS!");
        return;
    }
    iris_message_t* m = add_message(st, true, "i couldn't wake up: ");
    strcat(m->text, llm_status_message(status));
    strcat(m->text, ". my model goes on the disk as " CHAT_MODEL_FILE " and "
                    CHAT_TOKENIZER_FILE " (make chat-model, then make disk-models).");
}

static bool iris_idle(window_t* win) {
    iris_state* st = win->data;
    window_invalidate(win);
    if (!st->load_tried) {
        load_model(st);
        return false;
    }
    if (!st->chat) {
        return false;
    }
    chat_step_t step = chat_step(st->chat);
    strcpy(st->messages[st->message_count - 1].text, chat_reply(st->chat));
    return step != CHAT_STEP_DONE && step != CHAT_STEP_IDLE;
}

/* ---- input -------------------------------------------------------- */

static void iris_key(window_t* win, char key) {
    iris_state* st = win->data;
    if (key == '\n') {
        iris_send(win);
    } else if (key == KEY_UP) {
        st->scroll += SCROLL_STEP;
    } else if (key == KEY_DOWN) {
        st->scroll = st->scroll > SCROLL_STEP ? st->scroll - SCROLL_STEP : 0;
    } else {
        widget_textbox_key(&st->input_box, key);
    }
    window_invalidate(win);
}

static void iris_mouse(window_t* win, const event_t* e) {
    if (e->type == EVENT_MOUSE_DOWN && rect_contains(send_rect(), e->x, e->y)) {
        iris_send(win);
        window_invalidate(win);
    }
}

static void iris_close(window_t* win) {
    iris_state* st = win->data;
    chat_close(st->chat);
    kfree(st);
}

void iris_open(void) {
    iris_state* st = kmalloc(sizeof(iris_state));
    window_t* win = window_create("Iris", IRIS_WIDTH, IRIS_HEIGHT);
    if (!st || !win) {
        kfree(st);
        if (win) {
            window_close(win);
        }
        return;
    }
    memset(st, 0, sizeof(iris_state));
    st->input_box.text = st->input;
    st->input_box.capacity = sizeof(st->input);

    win->data = st;
    win->on_paint = iris_paint;
    win->on_key = iris_key;
    win->on_mouse = iris_mouse;
    win->on_idle = iris_idle;
    win->on_close = iris_close;
    win->draw_icon = iris_draw_icon;
    win->idle_work = true;          /* load the model on the first idle call */
}

void iris_draw_icon(surface_t* s, int x, int y) {
    avatar_draw(s, x, y, &iris_icon);
}
