/* Quickr's lexer's interface. How it works: qlex.c. */
#ifndef QR_QLEX_H
#define QR_QLEX_H
#include "qr/qast.h"

/* src's lines from first on to new tokens, src->toks: a QK_NL ending each statement and a QK_EOF
   the last; each error to e. */
void qlex_source(QSrc *src, uint32_t first, QMem *m, QErrs *e);
/* Keyword k's spelling (QK_AND .. QK_ASSERT); nullptr another kind. */
const char *qlex_keyword(QKind k);
#endif
