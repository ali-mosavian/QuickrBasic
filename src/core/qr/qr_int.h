/* What qr.c shares with the editor's other Quickr parts (qsugg.c). Not for outside src/core/qr:
   qr.h is. */
#ifndef QR_QR_INT_H
#define QR_QR_INT_H
#include "mem/program.h"
#include "qr/qast.h"
#include "qr/qpar.h"

enum { QR_LINE_CHUNK = 0x1000 }; /* a line lexed alone's arena's blocks */

/* Is record l a text line of file file's: Quickr's, not a run's own? */
bool qr_text(const Line *l, uint32_t file);
/* Is c a name's: a letter, a digit or _? */
bool qr_namec(char c);
/* The line text, len bytes, lexed alone in m: its tokens to *toks, their count, the line's end
   left out. */
uint32_t qr_lexed(QMem *m, const char *text, uint32_t len, const QTok **toks);
/* An empty program in m, its blocks' lines unpaired no error (QProg.loose). */
QProg qr_loose(QMem *m);
/* File file's lines, p's only module: lexed and parsed (p->loose), the errors sorted. */
void qr_lines(Session *S, QProg *p, QMem *m, uint32_t file);
/* File file's lines before record rec, p's only module (p made anew): lexed and parsed, where
   they leave off. */
QParAt *qr_upto(Session *S, QProg *p, QMem *m, uint32_t file, uint32_t rec);
#endif
