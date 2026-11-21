/* The editor's tests' fixture. How it works: edfix.c. */
#ifndef EDFIX_H
#define EDFIX_H
#include "session.h"

/* The test folder's file name: its path. */
const char *fx_path_of(const char *name);
void fx_write_file(const char *name, const char *text);
/* The test folder's file name's text. */
const char *fx_file_text(const char *name);
/* main.bas importing gfx.bas, opened. */
void fx_open_program(Session *S, const char *gfx);
/* The editor left, its files gone (names, nullptr ended), an empty program. */
void fx_done(Session *S, const char *const *names);
/* The text line, from the whole text's first, that is text, as LIST spells it; ~0 none. */
uint32_t fx_line_of(Session *S, const char *text);
/* Text line line typed over with text and entered, as the editor's Enter does: edit_commit's
   answer, false a syntax error (its box closed). */
bool fx_typed_over(Session *S, uint32_t line, const char *text);
void fx_retype(Session *S, uint32_t line, const char *text);
/* text's first word's column. */
uint16_t fx_col_of(const char *text, const char *word);
/* The cursor on the first `word` of the text line spelled `text`; at it. */
void fx_cursor_on(Session *S, const char *text, const char *word);
bool fx_cursor_at(Session *S, const char *text, const char *word);
#endif
