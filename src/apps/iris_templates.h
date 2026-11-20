/*
 * iris_templates.h - small assembly apps Iris can make from templates.
 *
 * Iris never writes code herself. She picks a template and fills its
 * slots (title, text, color, size); this file checks every value and
 * pastes it into a tested assembly program.
 */
#ifndef iris_templates_h__
#define iris_templates_h__

#include "lib/common.h"

#define TEMPLATE_TITLE_MAX  20      /* characters */
#define TEMPLATE_TEXT_MAX   32
#define TEMPLATE_SOURCE_MAX 4096    /* bytes of filled-in assembly */

typedef struct {
    const char* template_name;      /* "message", "box", "counter", "clock", "button" */
    const char* name;               /* the file's base name, e.g. "TAPS" */
    const char* title;              /* NULL = the template's default */
    const char* text;
    const char* color;              /* a color name, e.g. "blue" */
    const char* size;               /* "small", "medium" or "large" */
} template_request_t;

/* Fill in a template. Returns NULL on success, or a message saying which
   value was refused (unknown template or color, text too long, ...). */
const char* template_fill(const template_request_t* request, char* source, uint32_t capacity);

/* The 8.3 base name for the app: its title's letters and digits,
   uppercased ("My Clicks" -> "MYCLICKS"), or the template's name if there
   is no title, or "APP" if neither has any. 'base' needs 9 bytes. */
void template_base_name(const template_request_t* request, char* base);

/* For tests and docs: the templates' names, and a color's name by index
   (NULL past the last). */
const char* template_name_at(int index);
const char* template_color_at(int index);

#endif
