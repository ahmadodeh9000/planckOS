/*
 * chat.h - a conversation with Iris, one token at a time.
 *
 * Iris was trained on conversations in this format (training/my_chats.txt):
 *
 *   User: how do i list files
 *   Iris: `ls` lists the files on the disk with their sizes.
 *
 * chat_send() adds "User: <message>\nIris:" to the conversation, and
 * chat_step() runs the model once, until her reply line ends. Earlier
 * turns stay in the model's context, and the oldest ones are dropped when
 * the context (seq_len tokens) would overflow.
 */
#ifndef chat_h__
#define chat_h__

#include "llm/llm.h"

#define CHAT_MODEL_FILE     "IRIS.BIN"
#define CHAT_TOKENIZER_FILE "IRISTOK.BIN"
#define CHAT_MESSAGE_MAX    120     /* longest message, so a turn always fits */
#define CHAT_REPLY_MAX      256     /* longest reply kept, in bytes */

typedef struct chat chat_t;

typedef enum {
    CHAT_STEP_IDLE,         /* nothing to do: no message waiting */
    CHAT_STEP_READING,      /* read one token of the message */
    CHAT_STEP_WRITING,      /* wrote one token; chat_reply() has grown */
    CHAT_STEP_DONE,         /* the reply is complete */
} chat_step_t;

/* Load Iris's model from the disk. NULL on failure, with '*status' set. */
chat_t* chat_open(llm_status* status);
void chat_close(chat_t* chat);

/* Start Iris's reply to 'message' (cut at CHAT_MESSAGE_MAX). False if she
   is still busy with the previous one. */
bool chat_send(chat_t* chat, const char* message);

/* Do one token's worth of work. */
chat_step_t chat_step(chat_t* chat);

bool chat_busy(const chat_t* chat);

/* The reply so far (complete after CHAT_STEP_DONE). */
const char* chat_reply(const chat_t* chat);

#endif
