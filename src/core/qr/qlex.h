/* Quickr's lexer's interface. How it works: qlex.c. */
#ifndef QR_QLEX_H
#define QR_QLEX_H
#include "qr/qast.h"

/* src's lines from first on to new tokens, src->toks: a QK_NL ending each statement and a QK_EOF
   the last; each error to e. */
void qlex_source(QSrc *src, uint32_t first, QMem *m, QErrs *e);
/* A Quickr file's first statement, its mark: before it only blank lines and comments. */
#define QLEX_MARK "option dialect quickr"
/* Is the line text, len bytes, QLEX_MARK: case aside, spaces around it? */
bool qlex_mark(const char *text, uint32_t len);
/* Is the line text, len bytes, no statement: spaces, a comment? */
bool qlex_blank(const char *text, uint32_t len);
/* Do tokens first to n end in a lambda's header, fn(..) [as T], its fn not a type's (after as)?
   Its fn's index, n not. */
uint32_t qlex_header(const QTok *toks, uint32_t first, uint32_t n);
/* Keyword k's spelling (QK_AND .. QK_FUNCTION); nullptr another kind. */
const char *qlex_keyword(QKind k);
#endif
