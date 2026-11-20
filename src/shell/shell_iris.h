/*
 * shell_iris.h - the shell's `iris` command: a text conversation with Iris.
 *
 * While a session is open, the shell sends every typed line to Iris
 * instead of running it as a command. Her reply is written one token per
 * shell_iris_idle() call, so a Terminal window stays responsive.
 */
#ifndef shell_iris_h__
#define shell_iris_h__

#include "lib/console.h"

typedef struct shell_iris shell_iris_t;

/* Load the model and greet. NULL (with a message on 'con') on failure. */
shell_iris_t* shell_iris_start(console_t* con);
void shell_iris_end(shell_iris_t* session);

/* A typed line. Returns false if the user left (an empty line). */
bool shell_iris_line(shell_iris_t* session, const char* line);

/* True while Iris is writing (typed keys are ignored then). */
bool shell_iris_busy(const shell_iris_t* session);

/* One token of work. Returns true while there is more to do. */
bool shell_iris_idle(shell_iris_t* session);

#endif
