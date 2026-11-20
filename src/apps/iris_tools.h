/*
 * iris_tools.h - Iris's tools: she answers some requests with a tag, and
 * planckOS does the work.
 *
 *   [disk]  [memory]  [time]  [files]  [find "x"]  [open "NAME"]
 *   [make-app TEMPLATE title="..." text="..." color=C size=S]
 *
 * The model only chooses the tool and copies the values from your
 * message; the answer itself comes from the kernel (docs/LLM.md, "Tools").
 */
#ifndef iris_tools_h__
#define iris_tools_h__

#include "lib/common.h"
#include "lib/console.h"
#include "fs/fat12.h"

#define TOOL_VALUE_MAX  40
#define TOOL_SLOTS_MAX  4

typedef enum {
    TOOL_NONE,          /* a normal reply, no tag */
    TOOL_DISK,
    TOOL_MEMORY,
    TOOL_TIME,
    TOOL_FILES,
    TOOL_FIND,
    TOOL_OPEN,
    TOOL_MAKE_APP,
    TOOL_BAD,           /* looked like a tag, but isn't a valid one */
} tool_kind_t;

typedef struct {
    char key[8];                    /* title, text, color, size */
    char value[TOOL_VALUE_MAX];
} tool_slot_t;

typedef struct {
    tool_kind_t kind;
    char value[TOOL_VALUE_MAX];     /* find: the text; open: the file;
                                       make-app: the template */
    tool_slot_t slots[TOOL_SLOTS_MAX];
    int slot_count;
} tool_call_t;

/* Where the tool to run came from. */
typedef enum {
    TOOL_FROM_NOTHING,  /* no tool: show her reply */
    TOOL_FROM_MODEL,    /* her tag */
    TOOL_FROM_FALLBACK, /* she missed it; the keyword fallback found it */
    TOOL_FROM_BAD_TAG,  /* a broken tag and no fallback: say so kindly */
} tool_source_t;

typedef struct {
    bool can_open_windows;          /* false in the Terminal */
    char program[FAT12_NAME_MAX];   /* make-app sets the program it wrote */
} tool_env_t;

/* Read a tag from her reply. TOOL_NONE if it isn't one. */
tool_kind_t tool_parse(const char* reply, tool_call_t* call);

/* The keyword fallback: a tool for an obvious request in 'message'. */
bool tool_route(const char* message, tool_call_t* call);

/* Decide what to run for 'message' given her 'reply' (see tool_source_t). */
tool_source_t tool_choose(const char* message, const char* reply, tool_call_t* call);

/* Run the tool: a one-line answer goes to 'bubble', details to 'card'. */
void tool_run(const tool_call_t* call, tool_env_t* env, console_t* bubble, console_t* card);

/* The friendly reply for TOOL_FROM_BAD_TAG. */
#define TOOL_BAD_TAG_REPLY "i didn't get that, try rephrasing or type `help`."

#endif
