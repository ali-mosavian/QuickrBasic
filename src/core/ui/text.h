/* The text's interface. How it works: text.c. */
#ifndef UI_TEXT_H
#define UI_TEXT_H
#include "io/dialect.h"
#include "ui/ui.h"

/* tx_init: a new program of dialect d: one empty line, a text dialect's after its mark; the whole
   text the view; uimn.text.changed. */
void text_init(Session *S, const Dialect *d);
/* The program's dialect: a text one's lines their text (LF_TEXT). */
const Dialect *text_dialect(const Session *S);
/* Text line line (the whole text's) a text dialect's file's first, its mark: never changed. */
bool text_marker(Session *S, uint32_t line);
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
/* A change to the text, as uimn.text.edits is told it with the whole text's line: the line about
   to be replaced, replaced; inserted; about to be deleted. */
typedef enum : uint8_t { TE_REPLACING, TE_REPLACED, TE_INSERTED, TE_DELETING } TextEdit;
/* A line's file, and whether it is kept as typed. */
typedef struct {
    uint16_t file;
    bool asis;
} TextKind;
/* Text line line's (the whole text's, not the view's). */
TextKind text_kind(Session *S, uint32_t line);
/* The len characters at src text line line, before it (insert) or in its place: k's file's, as
   typed when k.asis. No view changes; uimn.text.edits is not told. */
void text_place(
    Session *S,
    uint32_t line,
    const uint8_t *src,
    uint16_t len,
    TextKind k,
    bool insert
);
/* Text line line out. No view changes; uimn.text.edits is not told. */
void text_remove(Session *S, uint32_t line);
/* Text line line's module (not the view's line). */
uint32_t text_module(Session *S, uint32_t line);
/* Module module changed since saved; then saved; none changed (a program opened). */
bool text_unsaved(Session *S, uint32_t module);
void text_saved(Session *S, uint32_t module);
void text_clean(Session *S);
/* tx_mark: line's mark (a breakpoint). */
bool text_mark(Session *S, uint32_t line);
/* tx_mtog: line's mark on, off. */
void text_toggle(Session *S, uint32_t line);
/* tx_mclr: no line marked, in the whole store. */
void text_unmark_all(Session *S);
/* Record rec's place among the store's marked lines: those before it. Each mark added or removed
   goes to uimn.text.marks by its place; the store emptied, all gone, as TEXT_MARKS. */
enum : uint32_t { TEXT_MARKS = UINT32_MAX };
uint32_t text_mark_nth(Session *S, uint32_t rec);
/* The nth marked line's mark off. */
void text_unmark(Session *S, uint32_t nth);

/* cb_put: len bytes at src the clipboard; lines: whole lines. Out of memory. */
void text_clip_put(Session *S, const uint8_t *src, uint16_t len, bool lines);
/* cb_lines: lines from to to (inclusive) the clipboard, each ended by NL, as many as fit. */
void text_clip_lines(Session *S, uint32_t from, uint32_t to);

/* tx_read: the file name the text, the whole the view: CR LF and LF end lines, a tab to the next
   column of 8, ^Z ends it, a long line is cut, a line ending in '_' goes on in the next (srcr's
   physical lines); each line as edit_canon makes it, a syntax error's kept as typed. A text
   dialect's file's lines as its Dialect.read reads them.
   PFIL_OK, else the DOS error of opening it, the text as it was. Errors: Out of memory. */
uint16_t text_read(Session *S, const char *name);
/* tx_write: module module's lines to the file name, ended by CR LF; PFIL_OK, else the DOS error
   (creating it, or the last write's). */
uint16_t text_write(Session *S, const char *name, uint32_t module);
#endif
