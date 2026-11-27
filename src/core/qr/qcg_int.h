/* Quickr's code generator's parts' shared interface: each part's functions the others call. Not
 * for outside qcg*.c: qcg.h is.
 *
 * # How it works
 * Each part owns one responsibility (its file's header says which). The emitting of ops, jumps
 * and loops (qcg_emit) calls nothing above it. Values, places, drops and chains call each other
 * as they nest: a value is built in its place, a place's old value drops, a chain is a value.
 * Generators and qcg.c's statements call down into them; nothing calls back up.
 */
#ifndef QR_QCG_INT_H
#define QR_QCG_INT_H
#include "qr/qcg.h"
#include "qr/qrt.h"
#include "vm/exec.h"

/* The int and single operators a right operand folds into: by form (plain, a variable's load, a
   literal's), then operator. */
enum { QA_ADD, QA_SUB, QA_MUL, QA_DIV, QA_AND, QA_OR };

/* A jump forward within the line, landed by qcg_land: a label's body, or a QJ's operand. */
typedef struct {
    uint32_t body;
    uint8_t *qj;
} QCgHop;

/* List place a's last element: a[len(a) - 1]'s parts, in len, one, last, at. */
typedef struct {
    QExpr len, one, last, at;
} QCgLast;

/* qcg_emit.c */
uint8_t qcg_pick(QType t, uint8_t i, uint8_t f, uint8_t d, uint8_t s);
bool qcg_par(const QVar *v);
void qcg_load(Code *c, const QVar *v);
void qcg_store(Code *c, const QVar *v);
void qcg_arith(Code *c, uint8_t *right, QType w, uint32_t k);
void qcg_xint(Code *c, QrtIop op, QType t, bool wrap);
QrtNum qcg_num(QType t); /* number type t as QX.INT, N.CONV and math's X ops take it */
void qcg_nconv(Code *c, QType from, QType to, bool wrap);
bool qcg_icell(QType t);
void qcg_compare(Code *c, QType w, QKind op, uint8_t *right);
void qcg_flush(QCg *g);
void qcg_target(QCg *g, const QStmt *s, uint32_t body, JumpMode mode);
void qcg_to(QCg *g, uint8_t op, const QStmt *s, uint32_t body, JumpMode mode);
uint8_t *qcg_qj(Code *c, QrtWhen when);
QCgHop qcg_hop(QCg *g, QrtWhen when);
void qcg_land(QCg *g, QCgHop h);
uint32_t qcg_top(QCg *g);
void qcg_back(QCg *g, QrtWhen when, uint32_t top);
void qcg_count(Code *c, QVar *i, QVar *lim, QVar *stp, int32_t by);
void qcg_bump(Code *c, QVar *v, uint32_t by);

/* qcg_expr.c */
void qcg_string(QCg *g, const QPiece *s);
QrtKind qcg_kind(QType t);
void qcg_as(QCg *g, const QExpr *e, QType w);
void qcg_hold(QCg *g, QType t, int32_t by);
void qcg_iarith(QCg *g, uint8_t *right, QType w, QKind op, bool wrap);
void qcg_expr(QCg *g, const QExpr *e);
void qcg_zero(QCg *g, QType t);

/* qcg_place.c */
const QTy *qcg_ty(const QCg *g, QType t);
bool qcg_rec(const QCg *g, QType t);
bool qcg_byplace(const QCg *g, QType t);
uint32_t qcg_size(const QCg *g, QType t);
QExpr qcg_named(QVar *v);
QExpr qcg_fieldof(const QCg *g, const QExpr *a, uint32_t k);
QExpr qcg_settle(QCg *g, const QExpr *e);
const QVar *qcg_copyat(const QCg *g, const QExpr *e);
void qcg_cell(QCg *g, const QExpr *e);
void qcg_loadat(QCg *g, const QExpr *e);
void qcg_copyrec(QCg *g, const QExpr *dst, const QExpr *src);
QExpr qcg_tried(QCg *g, const QExpr *e);
void qcg_ends(QCg *g, const QExpr *e);
void qcg_set(QCg *g, const QExpr *place, const QExpr *v);
void qcg_ref(QCg *g, const QExpr *a);
/* Array place a's descriptor, its cell's address pushed (a fixed array's made). */
void qcg_descriptor(QCg *g, const QExpr *a);
void qcg_pcall(QCg *g, const QFn *f);
void qcg_imports(QCg *g, const QStmt *s);
void qcg_call(QCg *g, const QExpr *e, const QExpr *dst);
void qcg_applied(QCg *g, const QExpr *e);
void qcg_x(Code *c, QrtX op);
void qcg_listop(QCg *g, const QExpr *e, const QExpr *dst);
void qcg_has(QCg *g, const QExpr *e);
void qcg_into(QCg *g, const QExpr *dst, const QExpr *e);
void qcg_fresh(QCg *g, QVar *v);

/* qcg_drop.c */
void qcg_lastof(QCgLast *l, const QExpr *a, QType elem);
bool qcg_drops(const QCg *g, QType t);
void qcg_drop(QCg *g, const QExpr *p, uint32_t depth);
void qcg_live(QCg *g, const QVar *v, bool on);
void qcg_dropvar(QCg *g, QVar *v);
void qcg_dropall(QCg *g, const QOwners *o);
void qcg_dropold(QCg *g, const QExpr *p);
void qcg_droplast(QCg *g, const QExpr *a, QType elem);
void qcg_moved(QCg *g, const QExpr *e);

/* qcg_chain.c */
void qcg_again(QCg *g, const QIter *it, uint32_t line);
void qcg_iter(QCg *g, const QExpr *e);
void qcg_chainfor(QCg *g, const QStmt *s);

/* qcg_generator.c */
void qcg_resume(QCg *g, const QFn *f);
void qcg_yield(QCg *g, const QStmt *s);
void qcg_done(QCg *g, const QFn *f);
void qcg_ended(QCg *g, const QStmt *s);
void qcg_genfor(QCg *g, const QStmt *s);

#endif
