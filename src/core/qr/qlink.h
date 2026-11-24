/* Quickr's link's interface. How it works: qlink.c. */
#ifndef QR_QLINK_H
#define QR_QLINK_H
#include "qr/qast.h"
#include "comp/comp.h"

/* Checked program p made ready to run: its fns procedures, its variables the symbol table's, each
   line compiled into the code cache, the frames laid out, the jumps resolved. dbg: a DBG op before
   each statement. */
void qlink_program(Comp *C, QProg *p, QMem *m, bool dbg);
/* Probe statement s (qsem_probe's) as the direct line's code, comp's: its variables from from on
   hidden module variables, which the next probe's take again; its line's text at text. False: it
   needs a record or a list of its own. */
bool qlink_direct(Comp *C, QProg *p, QMem *m, QVar *from, const QStmt *s, Addr text);
#endif
