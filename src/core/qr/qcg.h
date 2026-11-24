/* Quickr's code generator's interface. How it works: qcg.c. */
#ifndef QR_QCG_H
#define QR_QCG_H
#include "qr/qast.h"
#include "comp/code.h"
#include "mem/symbols.h"

/* What codegen needs besides the tree: where a line's text is in the arena (a string literal's
   load points there), the symbols (fusion's), the types (laid out), and whether a statement
   starts with DBG. */
typedef struct {
    const QProg *prog;
    Addr base;          /* the program store's */
    const QLine *lines; /* QLine.at: a line's record */
    const Symb *sy;
    bool dbg;
} QCgEnv;

/* A jump's target within a line: an entry of its statement map. */
typedef struct {
    uint32_t body; /* 1, 2 ..: not the statement's 0, under BOFF */
    uint32_t code; /* its offset in the line's code */
} QCgLabel;

/* A line's code in the making. The caller sets c, env, mem and line; qcg_stmt the rest. */
typedef struct {
    Code *c;
    const QCgEnv *env;
    QMem *mem;     /* the labels' room */
    uint32_t line; /* the line's record */
    uint8_t *seg;  /* the code from here not yet fused */
    QCgLabel *labels;
    uint32_t nlabels, caplabels;
    uint32_t hops;    /* the jumps within the line so far: their labels' bodies */
    uint32_t strings; /* strings on the stack under the value being made */
    const QOwners *own; /* the statement's frame's */
} QCg;

/* Scalar type t's ET_* in a record or a list: its own size's for a byte .. a ushort; a uint's
   ET_I4; a long's, a ulong's ET_R8. */
uint32_t qcg_scalar_et(QType t);
/* Statement s's code to g->c, fused, its labels in g->labels: a checked statement (qr/qsem.h),
   its variables and procedures linked. */
void qcg_stmt(QCg *g, const QStmt *s);
#endif
