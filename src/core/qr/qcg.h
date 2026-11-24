/* Quickr's code generator's interface. How it works: qcg.c. */
#ifndef QR_QCG_H
#define QR_QCG_H
#include "qr/qast.h"
#include "comp/code.h"

/* What codegen needs besides the tree: where a line's text is in the arena (a string literal's
   load points there), and whether a statement starts with DBG. */
typedef struct {
    Addr base;          /* the program store's */
    const QLine *lines; /* QLine.at: a line's record */
    bool dbg;
} QCgEnv;

/* Statement s's code to c: a checked one (qr/qsem.h), its variables and procedures linked. */
void qcg_stmt(Code *c, const QCgEnv *env, const QStmt *s);
#endif
