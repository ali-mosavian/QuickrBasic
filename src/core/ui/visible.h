/* The visible lines' interface. How it works: visible.c. */
#ifndef UI_VISIBLE_H
#define UI_VISIBLE_H
#include "ui/ui.h"

typedef struct {
    const uint8_t *text;
    uint32_t len;
    const uint8_t *cols; /* each character's slot; nullptr: highlighting off, or none kept */
} VislLine;

/* The text's line as drawn: its text and its colours, kept while its record is the same. */
VislLine visl_get(Session *S, uint32_t line);
#endif
