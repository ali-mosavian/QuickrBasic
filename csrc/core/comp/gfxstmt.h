/* The graphics statements' compiler's interface. How it works: gfxstmt.c. */
#ifndef COMP_GFXSTMT_H
#define COMP_GFXSTMT_H
#include "comp/comp.h"

/* The graphics statement at comp.p (FFh n) compiled; false not one. */
bool gstm_stmt(Comp *C);
#endif
