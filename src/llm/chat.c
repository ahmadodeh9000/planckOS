/*
 * chat.c - a conversation with Iris, one token at a time.
 *
 * The model keeps everything it has read in its KV cache, so each new
 * message only has to be fed once: "User: <message>\nIris:". Her reply
 * ends where she writes a newline, exactly like the training data, where
 * the next line would be "User: ...". That newline is left in the model
 * (llm_feed feeds it first), so the context reads like a training
 * conversation.
 *
 * When the next turn wouldn't fit in the context, the conversation is
 * rebuilt: the model starts over with as many of the latest turns as fit.
 * The same happens after a reply that didn't end cleanly (cut off, or
 * the model started writing "User:" itself), so the context is always a
 * well-formed conversation.
 */
#include "llm/chat.h"
#include "mm/kmalloc.h"
#include "lib/string.h"
#include "drivers/timer.h"
#include "drivers/serial.h"

#define CHAT_HISTORY        8       /* turns remembered for rebuilding */
#define REPLY_TOKENS_MAX    96      /* a reply this long is cut off (her longest is ~80) */
#define REPLY_RESERVE       96      /* context kept free for her reply */
#define CHAT_TEMPERATURE    0.3f    /* mostly the likeliest words, a little variety */
#define TURN_EXTRA          16      /* room for "User: ", "\nIris: " and '\n' */
#define NEXT_SPEAKER        "User:"

typedef enum {
    ENDING_NONE,        /* nothing in the context yet */
    ENDING_NEWLINE,     /* her reply ended with a newline (waiting in the model) */
    ENDING_PLAIN,       /* the model ended the text; the next turn needs a '\n' */
    ENDING_BROKEN,      /* cut off or garbled: rebuild before the next turn */
} chat_ending_t;

typedef struct {
    char message[CHAT_MESSAGE_MAX + 1];
    char reply[CHAT_REPLY_MAX];
} chat_turn_t;

struct chat {
    llm_t* llm;
    chat_turn_t turns[CHAT_HISTORY];    /* oldest first */
    int turn_count;
    char reply[CHAT_REPLY_MAX];
    int reply_length;
    int reply_tokens;
    uint32_t start_ticks;       /* when chat_send started this reply */
    bool busy;
    chat_ending_t ending;
};

chat_t* chat_open(llm_status* status) {
    chat_t* chat = kmalloc(sizeof(chat_t));
    if (!chat) {
        *status = LLM_NO_MEMORY;
        return NULL;
    }
    memset(chat, 0, sizeof(chat_t));
    *status = llm_load(CHAT_MODEL_FILE, CHAT_TOKENIZER_FILE, &chat->llm);
    if (*status != LLM_OK) {
        kfree(chat);
        return NULL;
    }
    llm_set_temperature(chat->llm, CHAT_TEMPERATURE);
    return chat;
}

void chat_close(chat_t* chat) {
    if (chat) {
        llm_free(chat->llm);
        kfree(chat);
    }
}

const char* chat_open_problem(llm_status status) {
    switch (status) {
        case LLM_NO_FILE:
            return "i can't find my brain file (" CHAT_MODEL_FILE " on the disk). run `make models` "
                   "on your computer and restart planckOS.";
        case LLM_NO_MEMORY:
            return "there isn't enough memory for my brain right now. close a window or "
                   "the Terminal's `iris` and try again.";
        case LLM_BAD_MODEL:
        case LLM_BAD_TOKENIZER:
            return "my brain file is damaged. run `make disk-models` on your computer to copy "
                   "a fresh one and restart planckOS.";
        default:
            return llm_status_message(status);
    }
}

bool chat_busy(const chat_t* chat) {
    return chat->busy;
}

const char* chat_reply(const chat_t* chat) {
    return chat->reply;
}

/* ---- building the context ----------------------------------------- */

/* Remember the new turn, forgetting the oldest one if the list is full. */
static void history_add(chat_t* chat, const char* message) {
    if (chat->turn_count == CHAT_HISTORY) {
        memmove(&chat->turns[0], &chat->turns[1], (CHAT_HISTORY - 1) * sizeof(chat_turn_t));
        chat->turn_count--;
    }
    chat_turn_t* turn = &chat->turns[chat->turn_count++];
    strcpy(turn->message, message);
    turn->reply[0] = '\0';
}

/* "User: m1\nIris: r1\n ... User: <message>\nIris:" with the last 'keep'
   remembered turns. */
static void build_prompt(chat_t* chat, int keep, const char* message, char* out) {
    out[0] = '\0';
    for (int i = chat->turn_count - keep; i < chat->turn_count; i++) {
        strcat(out, "User: ");
        strcat(out, chat->turns[i].message);
        strcat(out, "\nIris: ");
        strcat(out, chat->turns[i].reply);
        strcat(out, "\n");
    }
    strcat(out, "User: ");
    strcat(out, message);
    strcat(out, "\nIris:");
}

/* Start the model over with as many recent turns as fit. */
static bool rebuild(chat_t* chat, const char* message) {
    int capacity = (CHAT_HISTORY + 1) * (CHAT_MESSAGE_MAX + CHAT_REPLY_MAX + TURN_EXTRA);
    char* prompt = kmalloc(capacity);
    if (!prompt) {
        return false;
    }
    int room = llm_context_length(chat->llm) - REPLY_RESERVE;
    bool started = false;
    for (int keep = chat->turn_count; keep >= 0 && !started; keep--) {
        build_prompt(chat, keep, message, prompt);
        int tokens = llm_count_tokens(chat->llm, prompt, true);
        if (tokens >= 0 && tokens <= room) {
            started = llm_start(chat->llm, prompt);
        }
    }
    kfree(prompt);
    return started;
}

/* Add the turn after what the model has already read, if it fits. */
static bool append_turn(chat_t* chat, const char* message) {
    char text[CHAT_MESSAGE_MAX + TURN_EXTRA];
    strcpy(text, chat->ending == ENDING_PLAIN ? "\nUser: " : "User: ");
    strcat(text, message);
    strcat(text, "\nIris:");

    int tokens = llm_count_tokens(chat->llm, text, false);
    int room = llm_context_length(chat->llm) - REPLY_RESERVE - llm_tokens_used(chat->llm);
    return tokens >= 0 && tokens <= room && llm_feed(chat->llm, text);
}

/* Copy the message, cut to length, as one line of plain text. */
static void clean_message(const char* message, char* out) {
    int n = 0;
    for (; message[n] && n < CHAT_MESSAGE_MAX; n++) {
        char c = message[n];
        out[n] = (c < ' ' || c > '~') ? ' ' : c;
    }
    out[n] = '\0';
}

bool chat_send(chat_t* chat, const char* message) {
    if (chat->busy) {
        return false;
    }
    char clean[CHAT_MESSAGE_MAX + 1];
    clean_message(message, clean);

    bool ready = chat->ending != ENDING_NONE && chat->ending != ENDING_BROKEN &&
                 append_turn(chat, clean);
    if (!ready && !rebuild(chat, clean)) {
        return false;
    }
    history_add(chat, clean);
    chat->reply[0] = '\0';
    chat->reply_length = 0;
    chat->reply_tokens = 0;
    chat->start_ticks = timer_ticks();
    chat->busy = true;
    return true;
}

/* ---- writing the reply -------------------------------------------- */

static void finish(chat_t* chat, chat_ending_t ending) {
    uint32_t ms = (timer_ticks() - chat->start_ticks) * (1000 / TIMER_HZ);
    serial_printf("iris: replied in %u ms (%d tokens written, %d of %d context used)\n", ms,
                  chat->reply_tokens, llm_tokens_used(chat->llm), llm_context_length(chat->llm));
    chat->busy = false;
    chat->ending = ending;
    strcpy(chat->turns[chat->turn_count - 1].reply, chat->reply);
}

/* Add a piece of text; her first word comes with a space ("Iris: hi"). */
static void reply_append(chat_t* chat, const char* piece) {
    if (chat->reply_length == 0 && piece[0] == ' ') {
        piece++;
    }
    while (*piece && chat->reply_length < CHAT_REPLY_MAX - 1) {
        chat->reply[chat->reply_length++] = *piece++;
    }
    chat->reply[chat->reply_length] = '\0';
}

/* True (and the text removed) if she started writing the user's line. */
static bool reply_ends_with_speaker(chat_t* chat) {
    int n = strlen(NEXT_SPEAKER);
    if (chat->reply_length < n ||
        strcmp(chat->reply + chat->reply_length - n, NEXT_SPEAKER) != 0) {
        return false;
    }
    chat->reply_length -= n;
    while (chat->reply_length > 0 && chat->reply[chat->reply_length - 1] == ' ') {
        chat->reply_length--;
    }
    chat->reply[chat->reply_length] = '\0';
    return true;
}

chat_step_t chat_step(chat_t* chat) {
    if (!chat->busy) {
        return CHAT_STEP_IDLE;
    }
    const char* piece;
    llm_step_t step = llm_next(chat->llm, &piece);
    if (step == LLM_STEP_PROMPT) {
        return CHAT_STEP_READING;
    }
    if (step != LLM_STEP_TOKEN) {
        finish(chat, step == LLM_STEP_END ? ENDING_PLAIN : ENDING_BROKEN);
        return CHAT_STEP_DONE;
    }
    if (piece[0] == '\n') {
        finish(chat, ENDING_NEWLINE);       /* the newline waits in the model */
        return CHAT_STEP_DONE;
    }
    reply_append(chat, piece);
    chat->reply_tokens++;
    if (reply_ends_with_speaker(chat) || chat->reply_tokens >= REPLY_TOKENS_MAX) {
        finish(chat, ENDING_BROKEN);
        return CHAT_STEP_DONE;
    }
    return CHAT_STEP_WRITING;
}
