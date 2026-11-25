/* Quickr, the modern dialect (docs/language-design.md): running a Quickr file. How it works: qr.c. */
#ifndef QR_QR_H
#define QR_QR_H
#include "base.h"
#include "comp/link.h"
#include "mem/program.h"
#include "qr/qast.h"

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

/* A linked program's checked tree, kept for the debugger's probes till the next link or edit. */
typedef struct {
    QMem mem;
    QProg *prog;
    char said[80]; /* the last probe's error */
    void *sorts;   /* the merge sorts' buffers, the newest first (qrt.c) */
} QrKeep;
void qr_forget(QrKeep *k);

/* A QBasic program's Quickr modules (io/repl.c): read, then checked when its QBasic modules are
   declared, then linked in its link (comp/link.h), each's code run by its first IMPORT. Their
   tree, each line's statement and each module's exports, kept till the next load. */
typedef struct {
    LinkGuest link;
    QMem mem;
    QProg *prog;
    const QStmt *const *of;
    const struct Exports *ex; /* by QProg.mods */
} QrGuests;
/* The QBasic link's hooks; no tree. */
void qr_guest_init(QrGuests *g);
/* Module m's file, Quickr's, read into the store at its end; each module its imports name the
   program's, Quickr's, imported by m. */
void qr_guest_read(Session *S, uint32_t m);
/* Each Quickr module checked, every error reported (a file run's: then exit 1, else Syntax error
   raised); their records' TYPEs made, their exports declared as QBasic's, the link's hooks set. A
   module two of whose publics differ only by case: its ModuMod.clash. */
void qr_guest_declare(Session *S);
enum { QR_SPELL = 40 }; /* a name spelled: its bytes at most, as QBasic's */
/* Variable id's name as typed at d, QR_SPELL bytes at most (a block's over an outer one's,
   X'1's, too); its length, 0 not one of the kept program's. */
uint32_t qr_spell(const QrKeep *k, uint32_t id, char *d);
/* cons.inbuf's Quickr line as the direct line's code (comp's), its names as at program line rec's
   statement (0 or DLINE: the modules', after their last line); a value alone print()ed. The
   program linked (qr_link). An error: Syntax error raised, its message to qr.said. */
void qr_direct_code(Session *S, uint32_t rec);

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
/* Is the line text, len bytes, a function's first: its name's column and length (an operator's with its
   sign) to *at, *n. */
bool qr_fn_at(const char *text, uint32_t len, uint32_t *at, uint32_t *n);
/* The line text, len bytes, lexed alone: each token to fn with ctx, in order. Its comment's
   column, len none. Out of memory raises through f. */
typedef void QrTokFn(void *ctx, const QTok *t);
uint32_t qr_tokens(Fault *f, const char *text, uint32_t len, QrTokFn *fn, void *ctx);
/* A completion's kind: a keyword, a fn (rec its line), another name. */
typedef enum : uint8_t { QRS_KW, QRS_FN, QRS_NAME } QrSuggKind;
typedef void QrSuggFn(void *ctx, const char *text, uint32_t len, QrSuggKind kind, uint32_t rec);
/* What fits at a word whose line before it is text, n bytes, the line at rec (a new one's: the
   line it goes before), each to fn with ctx: after m. module m's public names; after v. the fields
   and methods of v's type; after T. type T's methods without self, an enum's values; else the
   keywords and the names in rec's file. None in a string or a comment. */
void qr_suggest(Session *S, uint32_t rec, const char *text, uint32_t n, QrSuggFn *fn, void *ctx);
#endif
