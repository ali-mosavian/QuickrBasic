/* Quickr's semantic pass's interface. How it works: qsem.c. */
#ifndef QR_QSEM_H
#define QR_QSEM_H
#include "qr/qast.h"

/* p's tree made valid: each name resolved, each type checked, conversions made explicit, constants
   folded; its variables on p->vars. Each error to p->errs: a tree without one compiles. */
void qsem_program(QProg *p, QMem *m);
/* Statement s, a probe's, checked where the names at are in sight (QStmt.at; null the modules'
   alone), p checked with keep: false an error, to p->errs. Its new variables after p's. */
bool qsem_probe(QProg *p, QMem *m, const QScopeAt *at, QStmt *s);
#endif
