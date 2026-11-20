/*
 * iris_facts.h - fact cards: exact help for commands and apps Iris is asked about.
 */
#ifndef iris_facts_h__
#define iris_facts_h__

#include "lib/console.h"

#define FACTS_MAX   3       /* cards per message */

/* Print one card line for every shell command or app named in 'message'
   (at most FACTS_MAX). Returns how many were printed. */
int facts_print(const char* message, console_t* out);

#endif
