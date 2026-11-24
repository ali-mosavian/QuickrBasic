/* Quickr's semantic pass's interface. How it works: qsem.c. */
#ifndef QR_QSEM_H
#define QR_QSEM_H
#include "qr/qast.h"

/* p's tree made valid: each name resolved, each type checked, conversions made explicit, constants
   folded; its variables on p->vars. Each error to p->errs: a tree without one compiles. */
void qsem_program(QProg *p, QMem *m);
#endif
