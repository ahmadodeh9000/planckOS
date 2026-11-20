/*
 * templates_test.c - writes every Iris app template, filled in, for
 * 'make templates-test'.
 *
 * For each template and each color it writes <template>_<color>.asm into
 * the directory given, with a title and text full of quotes and
 * semicolons, the values most likely to break the assembly. The Makefile
 * then assembles each file with the built-in assembler and with NASM and
 * compares the bytes. It also checks that bad values are refused.
 */
#include <stdio.h>
#include <string.h>
#include "apps/iris_templates.h"

#define SOURCE_MAX  TEMPLATE_SOURCE_MAX

static const char* const sizes[] = { "small", "medium", "large" };

static int write_filled(const char* dir, const char* name, const char* color, int i) {
    char source[SOURCE_MAX];
    template_request_t r = { name, "TEST", "Say \"hi\"; ok", NULL, color, NULL };
    /* Only the templates that have the slot may get it. */
    if (strcmp(name, "message") == 0) {
        r.text = "\"Quote\" ; not a comment";
    } else if (strcmp(name, "button") == 0) {
        r.text = "\"Push\"; me";        /* a button fits 20 */
    }
    if (strcmp(name, "box") == 0) {
        r.size = sizes[i % 3];
    }
    const char* problem = template_fill(&r, source, sizeof(source));
    if (problem) {
        fprintf(stderr, "templates-test: %s/%s refused: %s\n", name, color, problem);
        return 1;
    }
    char path[256];
    snprintf(path, sizeof(path), "%s/%s_%s.asm", dir, name, color);
    FILE* f = fopen(path, "w");
    if (!f) {
        perror(path);
        return 1;
    }
    fputs(source, f);
    fclose(f);
    return 0;
}

/* Values that must be refused, with the reason they must be. */
static int check_refused(void) {
    char source[SOURCE_MAX];
    const template_request_t bad[] = {
        { "rocket",  "X", NULL, NULL, NULL,     NULL },     /* unknown template */
        { "counter", "X", NULL, NULL, "plaid",  NULL },     /* unknown color */
        { "counter", "X", NULL, "hi", NULL,     NULL },     /* counter has no text */
        { "box",     "X", NULL, NULL, NULL,     "huge" },   /* unknown size */
        { "message", "X", "\n", NULL, NULL,     NULL },     /* not printable */
        { "message", "X", "A title that is far too long", NULL, NULL, NULL },
        { "button",  "X", NULL, "a label that won't fit", NULL, NULL },
    };
    int failures = 0;
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        if (!template_fill(&bad[i], source, sizeof(source))) {
            fprintf(stderr, "templates-test: bad request %u was accepted\n", i);
            failures++;
        }
    }
    return failures;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <output directory>\n", argv[0]);
        return 2;
    }
    int failures = check_refused();
    const char* name;
    for (int t = 0; (name = template_name_at(t)) != NULL; t++) {
        const char* color;
        for (int c = 0; (color = template_color_at(c)) != NULL; c++) {
            failures += write_filled(argv[1], name, color, c);
        }
    }
    return failures ? 1 : 0;
}
