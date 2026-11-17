/* The text's interface. How it works: text.c. */
#ifndef UI_TEXT_H
#define UI_TEXT_H
#include "ui/ui.h"

/* tx_init: a new program: one empty line, the whole text the view; uimn.text.changed. */
void text_init(Session *S);
/* tx_whole: the whole text the view. */
void text_whole(void);
/* tx_view: the view n lines from text line first. */
void text_view(uint32_t first, uint32_t n);
/* tx_rec: line's record (a program store offset). */
uint32_t text_rec(Session *S, uint32_t line);
/* The line of record rec to b's text, grown to fit; its length. Out of memory. */
uint32_t text_record(Session *S, uint32_t rec, LineBuf *b);
/* tx_get: line's text to b's, grown to fit; its length. Out of memory. */
uint32_t text_get(Session *S, uint32_t line, LineBuf *b);
/* tx_repl: line becomes the len characters at src, made a record as the editor stores it (its
   number, its body tokenized, LIST's spelling; as typed when uimn.text.asis, used up, or when
   that spelling is past LNMAX); its mark kept. */
void text_repl(Session *S, uint32_t line, const uint8_t *src, uint16_t len);
/* tx_insl: a new line of len characters at src before line (past the view's end: at its end);
   the hook told. */
void text_insert(Session *S, uint32_t line, const uint8_t *src, uint16_t len);
/* tx_dell: line out, the hook told; the view's last line left is emptied instead. */
void text_delete(Session *S, uint32_t line);
/* tx_mark: line's mark (a breakpoint). */
bool text_mark(Session *S, uint32_t line);
/* tx_mtog: line's mark on, off. */
void text_toggle(Session *S, uint32_t line);
/* tx_mclr: no line marked, in the whole store. */
void text_unmark_all(Session *S);

/* cb_put: len bytes at src the clipboard; lines: whole lines. Out of memory. */
void text_clip_put(Session *S, const uint8_t *src, uint16_t len, bool lines);
/* cb_lines: lines from to to (inclusive) the clipboard, each ended by NL, as many as fit. */
void text_clip_lines(Session *S, uint32_t from, uint32_t to);

/* tx_read: the file name the text, the whole the view: CR LF and LF end lines, a tab to the next
   column of 8, ^Z ends it, a long line is cut, a line ending in '_' goes on in the next (srcr's
   physical lines); each line as edit_canon makes it, a syntax error's kept as typed.
   PFIL_OK, else the DOS error of opening it, the text as it was. Errors: Out of memory. */
uint16_t text_read(Session *S, const char *name);
/* tx_write: the whole text to the file name, lines ended by CR LF; PFIL_OK, else the DOS error
   (creating it, or the last write's). */
uint16_t text_write(Session *S, const char *name);
#endif
