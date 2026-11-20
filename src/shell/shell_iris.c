/*
 * shell_iris.c - the shell's `iris` command: a text conversation with Iris.
 *
 *   admin$ iris
 *   iris> hi! ... (an empty line goes back to the shell)
 *   you> how do i list files
 *   iris> `ls` lists the files on the disk with their sizes.
 *   you>
 *   admin$
 *
 * Her reply is printed as it grows: after each chat_step(), whatever part
 * of chat_reply() hasn't been printed yet.
 */
#include "shell/shell_iris.h"
#include "llm/chat.h"
#include "mm/kmalloc.h"
#include "lib/string.h"

#define USER_PROMPT "you> "
#define IRIS_PROMPT "iris> "

struct shell_iris {
    console_t* con;
    chat_t* chat;
    uint32_t printed;       /* bytes of the current reply already shown */
};

shell_iris_t* shell_iris_start(console_t* con) {
    shell_iris_t* session = kmalloc(sizeof(shell_iris_t));
    if (!session) {
        console_print(con, "iris: out of memory\n");
        return NULL;
    }
    console_print(con, "iris: waking up...\n");
    llm_status status;
    session->chat = chat_open(&status);
    if (!session->chat) {
        console_printf(con, "iris: %s (put %s and %s on the disk: make disk-models)\n",
                       llm_status_message(status), CHAT_MODEL_FILE, CHAT_TOKENIZER_FILE);
        kfree(session);
        return NULL;
    }
    session->con = con;
    session->printed = 0;
    console_print(con, IRIS_PROMPT "hi! ask me anything about planckOS. "
                       "an empty line takes you back to the shell.\n" USER_PROMPT);
    return session;
}

void shell_iris_end(shell_iris_t* session) {
    chat_close(session->chat);
    kfree(session);
}

bool shell_iris_busy(const shell_iris_t* session) {
    return chat_busy(session->chat);
}

bool shell_iris_line(shell_iris_t* session, const char* line) {
    if (!*line) {
        console_print(session->con, IRIS_PROMPT "bye!\n");
        return false;
    }
    if (!chat_send(session->chat, line)) {
        console_print(session->con, IRIS_PROMPT "sorry, that didn't fit. try a shorter message.\n"
                                    USER_PROMPT);
        return true;
    }
    session->printed = 0;
    console_print(session->con, IRIS_PROMPT);
    return true;
}

/* Print the part of the reply that's new since the last call. */
static void print_new_text(shell_iris_t* session) {
    const char* reply = chat_reply(session->chat);
    uint32_t length = strlen(reply);
    if (length > session->printed) {
        console_print(session->con, reply + session->printed);
        session->printed = length;
    }
}

bool shell_iris_idle(shell_iris_t* session) {
    chat_step_t step = chat_step(session->chat);
    print_new_text(session);
    if (step == CHAT_STEP_DONE) {
        console_print(session->con, "\n" USER_PROMPT);
    }
    return chat_busy(session->chat);
}
