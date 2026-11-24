/* Quickr's link's interface. How it works: qlink.c. */
#ifndef QR_QLINK_H
#define QR_QLINK_H
#include "qr/qast.h"
#include "comp/comp.h"

/* Checked program p made ready to run: its fns procedures, its variables the symbol table's, each
   line compiled into the code cache, the frames laid out, the jumps resolved. dbg: a DBG op before
   each statement. */
void qlink_program(Comp *C, QProg *p, QMem *m, bool dbg);
#endif
