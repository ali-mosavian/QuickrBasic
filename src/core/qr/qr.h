/* Quickr, the modern dialect (docs/language-design.md): running a Quickr file. How it works: qr.c. */
#ifndef QR_QR_H
#define QR_QR_H
#include "base.h"
#include "mem/program.h"

/* A Quickr file's first line, its dialect's mark; any other file is QBasic's. */
extern const char qr_dialect[];
/* Is the line text, len bytes, qr_dialect: case aside, spaces after it? */
bool qr_marker(const char *text, uint32_t len);
/* Is the file at path Quickr's: its first line qr_dialect? */
bool qr_is_file(const char *path);
/* Load repl.file, Quickr's, and report its compile errors; none: link it, ready to run. The errors'
   count. */
uint32_t qr_build(Session *S);
/* Run repl.file, Quickr's: every compile error reported, then exit 1; --check (repl.checking): exit
   after the check. */
void qr_run(Session *S);
/* Runtime error code at a Quickr line (vm.line): file:line: message, then the line. */
void qr_report(Session *S, uint8_t code);
/* Runtime error code's message, a Quickr line's: assert's its own. */
const char *qr_message(const Session *S, uint8_t code);

/* The editor's: a Quickr program's lines in the store (LF_QR), each its file's, the main's 0 and
   a module's its module's number (mem/module.h). */

/* An error at a stored line: its record, its column; its message to msg, cap bytes with its 0. */
typedef struct {
    uint32_t rec, col;
    char *msg;
    uint32_t cap;
} QrErr;

/* The file at path's lines after the store's, Quickr's, of file file: tabs to columns of 8, ^Z its
   end. PFIL_OK, else the DOS error. */
uint16_t qr_read(Session *S, const char *path, uint16_t file);
/* The store's program, repl.file the main's: each module its lines import read in after the rest,
   once; one not found left to qr_link. A module no import reaches dropped, unless edited. */
void qr_open(Session *S);
/* qr_open's, then checked and linked, ready to run (stmt_runfirst): its errors' count, the first
   to *e. */
uint32_t qr_link(Session *S, QrErr *e);
/* The program qr_link made run from its first line, under repl_call: as repl_run. */
[[noreturn]]
void qr_go(Session *S);
/* Record rec's file's lines lexed and parsed, a block's lines unpaired no error: rec's first error
   to *e; false none. */
bool qr_syntax(Session *S, uint32_t rec, QrErr *e);
/* Each stored Quickr line LF_ASIS when qr_syntax finds an error in it, else not. */
void qr_flag(Session *S);
/* The line a run's copy (LF_INC) was made from; else rec. */
uint32_t qr_origin(const Prog *pg, uint32_t rec);
/* The run's copies of the line at rec breakpoints as it is (LF_BP). */
void qr_marks(Prog *pg, uint32_t rec);
/* Is the line text, len bytes, a fn's first: its name's column and length (an operator's with its
   sign) to *at, *n. */
bool qr_fn_at(const char *text, uint32_t len, uint32_t *at, uint32_t *n);
#endif
