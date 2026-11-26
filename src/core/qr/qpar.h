/* Quickr's parser's interface. How it works: qpar.c. */
#ifndef QR_QPAR_H
#define QR_QPAR_H
#include "qr/qast.h"

/* Module mod's tokens to statements and fns, after p's: mod->head .. mod->tail; each error to
   p->errs. */
void qpar_program(QProg *p, QMem *m, QMod *mod);
/* Fn f's tokens parsed again, a fresh tree: its statements at the program's end, the fn last of
   its fns. A generic's instance starts so. */
QFn *qpar_again(QProg *p, QMem *m, const QFn *f);
/* Type block s's tokens parsed again, a fresh tree: its statements and methods at the program's
   end, its type statement returned. A generic record's instance starts so. */
QStmt *qpar_typeagain(QProg *p, QMem *m, const QStmt *s);
/* The expression at token t parsed again, a fresh tree: a one-line lambda's body where it is
   inlined. Null after an error, which the first parse said. */
QExpr *qpar_reexpr(QProg *p, QMem *m, const QTok *t);
/* The line at token t as one simple statement (a call, an assignment ..), on no list. Null after
   an error, to p->errs. */
QStmt *qpar_line(QProg *p, QMem *m, const QTok *t);

/* The editor's completion: what a line would make where a module's lines so far leave off. */
typedef struct QParAt QParAt;
typedef enum : uint8_t {
    QFIT_NO,
    QFIT_YES,
    QFIT_CLOSES, /* it ends the innermost open block, or adds a clause: else, case */
} QFit;
/* Module mod's tokens parsed (p->loose), in m: where they leave off. */
QParAt *qpar_upto(QProg *p, QMem *m, const QMod *mod);
/* Tokens line, to a QK_NL then a QK_EOF, one statement parsed where a leaves off: whether its
   token word fits, no error at or before it, and closes. a as it was after. */
QFit qpar_fits(QParAt *a, const QTok *line, const QTok *word);
#endif
