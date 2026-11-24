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
/* The expression at token t parsed again, a fresh tree: a one-line lambda's body where it is
   inlined. Null after an error, which the first parse said. */
QExpr *qpar_reexpr(QProg *p, QMem *m, const QTok *t);
/* The line at token t as one simple statement (a call, an assignment ..), on no list. Null after
   an error, to p->errs. */
QStmt *qpar_line(QProg *p, QMem *m, const QTok *t);
#endif
