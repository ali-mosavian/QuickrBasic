/* Undo and redo's interface. How it works: undo.c. */
#ifndef UI_UNDO_H
#define UI_UNDO_H
#include "ui/ui.h"

/* The text's changes kept from now (uimn.text.edits). */
void undo_init(void);
/* No steps kept: a new or opened program. */
void undo_clear(void);
/* A command starts; it ended: its changes one step. */
void undo_begin(Session *S);
void undo_end(Session *S);
/* The last step undone; the one undone last done again. false: none. */
bool undo_undo(Session *S);
bool undo_redo(Session *S);
#endif
