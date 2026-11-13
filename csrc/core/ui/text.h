/* The editor's text: the program store's lines; the clipboard; files (src/ui/text.asm).

   The text is the program store: a line a record, uimn.text.all of them, at least one, in
   order, each found by the line map (lmap.h). A line is its record's pre bytes (its indent and
   number as typed), then its body as LIST spells it; an LF_ASIS line is its pre alone (a line
   with a syntax error, kept as typed; uimn.text.asis asks it of the next line stored). A line's
   mark (a breakpoint) is its LF_BP. The clipboard: uimn.text.clip_len bytes of
   uimn.text.clip, whole lines (clip_lines, each ended by NL) or a piece of one.

   The editor sees a view: uimn.text.n lines from line uimn.text.first, numbered from 0 (a
   procedure's lines, view.h). Lines below are the view's unless said. A line added or removed
   goes to uimn.text.hook (the text's line, 1 or -1). Storing a line can raise Out of memory. */
#ifndef UI_TEXT_H
#define UI_TEXT_H
#include "ui/ui.h"

/* tx_init: a new program: one empty line, the whole text the view; uimn.text.changed. */
void text_init(void);
/* tx_whole: the whole text the view. */
void text_whole(void);
/* tx_view: the view n lines from text line first. */
void text_view(uint32_t first, uint32_t n);
/* tx_rec: line's record (a program store offset). */
uint32_t text_rec(uint32_t line);
/* tx_get: line's text to dst (LNBUF bytes room); its length. */
uint32_t text_get(uint32_t line, uint8_t *dst);
/* tx_repl: line becomes the len characters at src, made a record as the editor stores it (its
   number, its body tokenized, LIST's spelling; as typed when uimn.text.asis, used up, or when
   that spelling is past LNMAX); its mark kept. */
void text_repl(uint32_t line, const uint8_t *src, uint16_t len);
/* tx_insl: a new line of len characters at src before line (past the view's end: at its end);
   the hook told. */
void text_insert(uint32_t line, const uint8_t *src, uint16_t len);
/* tx_dell: line out, the hook told; the view's last line left is emptied instead. */
void text_delete(uint32_t line);
/* tx_mark: line's mark (a breakpoint). */
bool text_mark(uint32_t line);
/* tx_mtog: line's mark on, off. */
void text_toggle(uint32_t line);
/* tx_mclr: no line marked, in the whole store. */
void text_unmark_all(void);

/* cb_put: len bytes at src the clipboard; lines: whole lines. */
void text_clip_put(const uint8_t *src, uint16_t len, bool lines);
/* cb_lines: lines from to to (inclusive) the clipboard, each ended by NL, as many as fit. */
void text_clip_lines(uint32_t from, uint32_t to);

/* tx_read: the file name the text, the whole the view: CR LF and LF end lines, a tab to the next
   column of 8, ^Z ends it, a long line is cut, a line ending in '_' goes on in the next (srcr's
   physical lines); each line as edit_canon makes it, a syntax error's kept as typed.
   PFIL_OK, else the DOS error of opening it, the text as it was. Errors: Out of memory. */
uint16_t text_read(const char *name);
/* tx_write: the whole text to the file name, lines ended by CR LF; PFIL_OK, else the DOS error
   (creating it, or the last write's). */
uint16_t text_write(const char *name);
#endif
