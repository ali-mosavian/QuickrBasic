/* Editing: a line buffer, the program window, the selection, the clipboard, the immediate window
   (src/ui/edit.asm).

   The program window edits its line uimn.ed.line in uimn.edline and stores it when the cursor
   leaves (edit_commit): spaces at its end cut, keywords capitalised, the syntax checked. A
   selection from (sel_line, sel_col) to the cursor is a piece of one line, or whole lines when it
   spans several. The insert mode, uimn.ed.insert, is every line buffer's. */
#ifndef UI_EDIT_H
#define UI_EDIT_H
#include "ui/ui.h"
#include "vm/exec.h"

/* edit_sel's, edit_copy's */
enum : uint8_t { SEL_NONE = 0, SEL_PIECE = 1, SEL_LINES = 2 };

/* A line buffer b (le_*). */
/* le_ins: c typed at the cursor, inserted or over the character there (padded first); false:
   full (the cursor at most, or the text at most in insert mode), nothing done. */
bool edit_lb_type(LineBuf *b, uint8_t c, uint16_t most);
/* le_insn: n characters of s at the cursor, the cursor past them; false: past LNMAX, nothing
   done. */
bool edit_lb_insert(LineBuf *b, const uint8_t *s, uint16_t n);
/* le_cut: characters from to to (exclusive, to cut to the length) out; the cursor to from. */
void edit_lb_cut(LineBuf *b, uint16_t from, uint16_t to);
/* le_bs: Backspace; false: at column 0, nothing done. */
bool edit_lb_bs(LineBuf *b);
/* le_del: Del; false: at or past the end, nothing done. */
bool edit_lb_del(LineBuf *b);
/* le_home: to the first non-blank, or to column 0 when there. */
void edit_lb_home(LineBuf *b);
/* le_end: past the last character. */
void edit_lb_end(LineBuf *b);
/* le_left, le_right: a column left; right, LNMAX at most. */
void edit_lb_left(LineBuf *b);
void edit_lb_right(LineBuf *b);
/* le_wleft, le_wright: to the word before; the next word (words are split by spaces). */
void edit_lb_wleft(LineBuf *b);
void edit_lb_wright(LineBuf *b);

/* The program window. */
/* ed_load: line into uimn.edline, not dirty; the column stays. */
void edit_load(uint32_t line);
/* ed_commit: a dirty line back to the text (above), uimn.text.changed, unsaved; a SUB or
   FUNCTION line typed gets its view (view_commit). false: a syntax error, shown in a box with its
   word selected; the line is stored as typed. */
bool edit_commit(void);
/* ed_goto: the cursor to line, this one committed first; false: it stayed (a syntax error). */
bool edit_goto(uint32_t line);
/* ed_up, ed_down, ed_pgup, ed_pgdn, ed_first, ed_last: the cursor a line up, down, a window up,
   down; to the first line (column 0), the last (past its end). */
void edit_up(void);
void edit_down(void);
void edit_pgup(void);
void edit_pgdn(void);
void edit_first(void);
void edit_last(void);
/* ed_ensure: uimn.ed.top and left so that the cursor shows. */
void edit_ensure(void);
/* ed_enter: Enter: the line split at the cursor, the new line indented as this one. */
void edit_enter(void);
/* ed_bs: Backspace: at column 0 the line joins the one above. */
void edit_bs(void);
/* ed_del: Del: past the end the next line joins this one. */
void edit_del(void);
/* ed_tab: Tab: spaces to the next column of 8. */
void edit_tab(void);
/* ed_ctrly: Ctrl+Y: the line to the clipboard, out of the text. */
void edit_ctrly(void);

/* sel_mark: a shifted move: the selection starts at the cursor if not on yet. */
void edit_sel_mark(void);
/* sel_get: the selection: SEL_PIECE (uimn.ed.sel_a to sel_b columns of the cursor's line),
   SEL_LINES (sel_a to sel_b lines; a lower end at column 0 not its line's), SEL_NONE. */
uint8_t edit_sel(void);
/* ed_copy: the selection to the clipboard; edit_sel's. */
uint8_t edit_copy(void);
/* ed_clear: the selection out of the text; off. */
void edit_clear(void);
/* ed_cut: edit_copy, edit_clear. */
void edit_cut(void);
/* ed_paste: the clipboard at the cursor, over the selection: a piece into the line, lines
   before it. */
void edit_paste(void);

/* The immediate window. */
/* im_load: line uimn.imm.cur into uimn.immline. */
void edit_imm_load(void);
/* im_up, im_down: the line before, after. */
void edit_imm_up(void);
void edit_imm_down(void);
/* im_next: Enter: the line kept, its text in cons.inbuf; the cursor to the next line, a new one
   at the end, the oldest dropped when IMN are kept. The text's length. */
uint16_t edit_imm_next(void);

/* Lines as the editor stores them. */
/* ui_body: past text's indent, its number and the blanks after: at its body. */
uint8_t *edit_body(uint8_t *text);
/* ui_canon: the len characters at text (text[len] is set to 0) as LIST shows it, its label and
   indent kept, in cons.sbuf, cons.sblen long (as typed when past LNMAX or bodiless); the body's
   declarations entered (decl_line), checked when uimn.text.check. false: a syntax error,
   uimn.errmsg its message, edit's error column set for edit_commit; sbuf not made. */
bool edit_canon(uint8_t *text, uint16_t len);
/* ui_dxerr: a compile error's words in uimn.errmsg: its class, what is wrong, then what to do,
   wrapped for a box (diag's). text the line's, 0-ended; op its ERR op. The column in text it is
   at. Uses cons.inbuf and comp_cbuf. */
uint16_t edit_dxerr(const uint8_t *text, const ErrOp *op);
/* ui_emsg: error code's message (emsg_text) in uimn.errmsg. */
void edit_emsg(uint8_t code);
#endif
