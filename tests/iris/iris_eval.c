/*
 * iris_eval.c - measures Iris on a normal computer, for 'make iris-eval'.
 *
 * Built from the kernel's own src/llm/ and src/apps/iris_tools.c (with the
 * shims in tests/iris/host/), so it runs exactly the code the Iris app
 * runs: chat_send/chat_step for her reply, then tool_choose to decide
 * which tool runs. The tools themselves aren't run, only chosen.
 *
 *   iris_eval tools training/tool_test.txt   score the held-out requests
 *   iris_eval chat  FILE                     print conversations: each
 *                                            line is a message, a blank
 *                                            line starts a new conversation
 */
#include <stdio.h>
#include <string.h>
#include "llm/chat.h"
#include "apps/iris_tools.h"

#define LINE_MAX_LEN    512
#define TAG_MAX         256

/* One canonical text for a call, so two calls compare with strcmp. */
static void describe(const tool_call_t* call, char* out) {
    static const char* const names[] = { "none", "disk", "memory", "time", "files", "find",
                                         "open", "make-app", "bad" };
    static const char* const keys[] = { "title", "text", "color", "size" };
    sprintf(out, "[%s", names[call->kind]);
    if (call->kind == TOOL_FIND || call->kind == TOOL_OPEN) {
        sprintf(out + strlen(out), " \"%s\"", call->value);
    } else if (call->kind == TOOL_MAKE_APP) {
        sprintf(out + strlen(out), " %s", call->value);
        for (int k = 0; k < 4; k++) {
            for (int i = 0; i < call->slot_count; i++) {
                if (strcmp(call->slots[i].key, keys[k]) == 0) {
                    sprintf(out + strlen(out), " %s=%s", keys[k], call->slots[i].value);
                }
            }
        }
    }
    strcat(out, "]");
}

/* Her reply to one message, in a fresh conversation. */
static const char* ask(const char* message) {
    static chat_t* chat;
    llm_status status;
    chat_close(chat);
    chat = chat_open(&status);
    if (!chat) {
        fprintf(stderr, "iris_eval: %s\n", llm_status_message(status));
        return NULL;
    }
    chat_send(chat, message);
    while (chat_step(chat) != CHAT_STEP_DONE) {
    }
    return chat_reply(chat);
}

typedef struct {
    int total, model_right, fallback_used, fallback_right, final_right;
} score_t;

static void score_one(const char* message, const char* expected_tag, score_t* score) {
    tool_call_t expected, chosen, from_model;
    char want[TAG_MAX], got[TAG_MAX], model_tag[TAG_MAX];
    tool_parse(expected_tag, &expected);
    describe(&expected, want);

    const char* reply = ask(message);
    tool_parse(reply, &from_model);
    describe(&from_model, model_tag);
    tool_source_t source = tool_choose(message, reply, &chosen);
    describe(&chosen, got);
    if (source == TOOL_FROM_NOTHING || source == TOOL_FROM_BAD_TAG) {
        strcpy(got, "(no tool)");
    }

    bool model_ok = strcmp(model_tag, want) == 0;
    bool final_ok = strcmp(got, want) == 0;
    score->total++;
    score->model_right += model_ok;
    score->final_right += final_ok;
    if (source == TOOL_FROM_FALLBACK) {
        score->fallback_used++;
        score->fallback_right += final_ok;
    }
    printf("%-4s %-44s model: %-40s %s%s\n", final_ok ? "ok" : "MISS", message, reply,
           source == TOOL_FROM_FALLBACK ? "fallback: " : "", source == TOOL_FROM_MODEL ? "" : got);
}

static int eval_tools(FILE* f) {
    char line[LINE_MAX_LEN];
    score_t score = { 0, 0, 0, 0, 0 };
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        char* tab = strchr(line, '\t');
        if (line[0] == '#' || !tab) {
            continue;
        }
        *tab = '\0';
        score_one(line, tab + 1, &score);
    }
    printf("\nrequests:                     %d\n", score.total);
    printf("the model's tag was right:    %d (%d%%)\n", score.model_right,
           100 * score.model_right / score.total);
    printf("the keyword fallback stepped in: %d times, right %d times\n",
           score.fallback_used, score.fallback_right);
    printf("the right tool ran in the end: %d (%d%%)\n", score.final_right,
           100 * score.final_right / score.total);
    return 0;
}

static int print_chats(FILE* f) {
    char line[LINE_MAX_LEN];
    chat_t* chat = NULL;
    llm_status status;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (!chat || !line[0]) {
            chat_close(chat);
            chat = chat_open(&status);
            printf("\n");
            if (!line[0]) {
                continue;
            }
        }
        chat_send(chat, line);
        while (chat_step(chat) != CHAT_STEP_DONE) {
        }
        printf("User: %s\nIris: %s\n", line, chat_reply(chat));
    }
    chat_close(chat);
    return 0;
}

int main(int argc, char** argv) {
    FILE* f = argc == 3 ? fopen(argv[2], "r") : NULL;
    if (!f) {
        fprintf(stderr, "usage: %s tools|chat <file>\n", argv[0]);
        return 2;
    }
    int result = strcmp(argv[1], "tools") == 0 ? eval_tools(f) : print_chats(f);
    fclose(f);
    return result;
}
