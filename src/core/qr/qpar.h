/* Quickr's parser's interface. How it works: qpar.c. */
#ifndef QR_QPAR_H
#define QR_QPAR_H
#include "qr/qast.h"

/* p's tokens (p->src) to its statements and fns; each error to p->errs. */
void qpar_program(QProg *p, QMem *m);
#endif
