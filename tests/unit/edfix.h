/* The editor's tests' fixture. How it works: edfix.c. */
#ifndef EDFIX_H
#define EDFIX_H
#include "session.h"

/* Keys: their scan code and character. */
enum : uint16_t {
    FX_ENTER = 0x1C0D,
    FX_ESC = 0x011B,
    FX_TAB = 0x0F09,
    FX_BACKSPACE = 0x0E08,
    FX_UP = 0x4800,
    FX_DOWN = 0x5000,
    FX_LEFT = 0x4B00,
    FX_RIGHT = 0x4D00,
    FX_HOME = 0x4700,
    FX_END = 0x4F00,
    FX_DEL = 0x5300,
    FX_CTRL_Y = 0x1519,
    FX_CTRL_Z = 0x2C1A,
};

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
/* key run as the key loop runs it, the shift flags flags (PKEY_*); each character of text. */
void fx_key(Session *S, uint16_t key, uint8_t flags);
void fx_type(Session *S, const char *text);
/* The cursor's line as edited, 0-ended. */
const char *fx_line(void);
#endif
