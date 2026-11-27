/* Quickr's semantic pass's interface. How it works: qsem.c. */
#ifndef QR_QSEM_H
#define QR_QSEM_H
#include "qr/qast.h"

/* p's tree made valid: each name resolved, each type checked, conversions made explicit, constants
   folded; its variables on p->vars. Each error to p->errs: a tree without one compiles. By stages,
   for modules of two dialects that import each other's: begun, then each stage's modules (in, by
   module number) checked once the stages' before are, a QBasic one's exports bound (QMod.ex),
   then ended. */
typedef struct QSem QSem;
QSem *qsem_start(QProg *p, QMem *m);
void qsem_check(QSem *S, const bool *in);
void qsem_finish(QSem *S);
/* Statement s, a probe's, checked where the names at are in sight (QStmt.at; null the modules'
   alone), p checked with keep: false an error, to p->errs. Its new variables after p's. */
bool qsem_probe(QProg *p, QMem *m, const QScopeAt *at, QStmt *s);
/* What has a built-in method: a list, a fixed array, a dict, a string. */
enum : uint8_t { QM_LIST = 1, QM_ARRAY = 2, QM_DICT = 4, QM_STRING = 8 };
typedef void QsemMethodFn(void *ctx, const char *name);
/* Each built-in method a value of the kinds on has, its name to fn with ctx: its own, then a
   chain's. */
void qsem_methods(uint8_t on, QsemMethodFn *fn, void *ctx);
/* Module m's statement after s, past a fn's: null after its last. */
QStmt *qsem_nextin(const QMod *m, QStmt *s);
#endif
