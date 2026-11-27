/* Quickr's semantic pass's parts' shared interface: its state, and each part's functions the
 * others call. Not for outside qsem*.c: qsem.h is.
 *
 * # How it works
 * Each part owns one responsibility (its file's header says which). The errors' text (qsem_say)
 * and names (qsem_scope) call nothing above them. The rest, types, expressions, statements and
 * declarations, call each other as the language nests them: an array's count is an expression,
 * a lambda's body statements, a conversion to T? a move. Modules (qsem_mod) and qsem.c's passes
 * call down into them; nothing calls back up.
 */
#ifndef QR_QSEM_INT_H
#define QR_QSEM_INT_H
#include "qr/qsem.h"

/* A name: a variable's, a fn's, a type's, or a module's (scope its QScope + 1); from m import X's
   alias, X found in m's scope when used. */
typedef struct {
    const QTok *name;
    QVar *var;
    QFn *fn;
    QType ty; /* a type's name */
    uint32_t scope;
    const QTok *alias;
    bool pub;
    const char *why; /* a QBasic module's name Quickr can't use: why, said where it is used */
} QBind;

/* A module's names, or a package folder's modules (mod QMOD_NONE); ready once its statements are
   checked, its vars bound. */
typedef struct {
    QBind *at;
    uint32_t n, cap;
    uint32_t mod;
    bool ready;
} QScope;

/* A for over a list: the list's variable, borrowed till its next. */
typedef struct {
    const QVar *root;
    const QStmt *loop;
} QLoan;

/* A match open: its values seen, an enum's or a bool's. */
typedef struct {
    QStmt *s;
    uint8_t *seen;
    uint32_t nseen;
    bool other; /* case else */
} QOpenMatch;

/* A lambda being checked at its site: its scope from binding mark on; up, the one it is in. */
typedef struct QLamAt QLamAt;

struct QLamAt {
    QLam *lam;
    uint32_t mark;
    QLamAt *up;
    QBind **views; /* each of lam's captures as its body sees it */
    uint32_t capviews;
};

/* A variable moved: a copy if it is used after, or in a loop it is declared outside. */
typedef struct {
    QExpr *e;
    QVar *var;
    uint32_t at, loop; /* their tokens; loop UINT32_MAX none */
} QMove;

/* A call lending a module var, checked once every fn's writes are known; in, the fn it is in. */
typedef struct {
    const QExpr *call;
    QFn *in;
} QLend;

struct QSem {
    QProg *prog;
    QMem *mem;
    QBind *binds;
    uint32_t nbinds, capbinds;
    uint32_t *marks;
    uint32_t nmarks, capmarks;
    QFn *fn; /* the fn checked, null the module */
    QVar *lastvar;
    bool early; /* the module's consts checked, before its other names are bound */
    QLoan *loans;
    uint32_t nloans, caploans;
    QOpenMatch *matches;
    uint32_t nmatches, capmatches;
    QMove *moves;
    uint32_t nmoves, capmoves;
    uint32_t *loops; /* the open loops' first tokens */
    uint32_t nloops, caploops;
    QLamAt *lamat;           /* the lambda checked at its site, null none */
    uint32_t hidelo, hidehi; /* bindings not seen: a fn's, while a lambda's body is checked in it */
    bool passing;   /* an argument for a fn parameter checked: a lambda bound may pass on */
    QStmt *genfor;  /* the next generator instance: for x in g(..)'s, inline in this for */
    bool genkept;   /* the next generator instance: var it = g(..)'s, resumable */
    QScope *scopes; /* each module's, by number, then the package folders' */
    uint32_t nscopes, capscopes;
    bool pub; /* a public statement's names bound */
    QLend *lends;
    uint32_t nlends, caplends;
    QFn **values; /* fns named as values: what a call through one may run */
    uint32_t nvalues, capvalues;
    uint32_t walks;  /* qsem_writer's searches */
    QScopeAt *at;    /* the last statement's names in sight, QProg.keep */
    QType inst;      /* a generic record's instance being made, its errors named for it */
    const QFn *body; /* the method whose body is inlined, its errors named for it; null none */
    uint32_t site;   /* the call body is inlined at: where its uses and moves are (qsem_useat) */
};

/* The names in sight at a statement: its fn's, a block's, the modules' (QSemKeep's). */
struct QScopeAt {
    QFn *fn;
    QBind *binds;
    uint32_t n;
};

struct QSemKeep {
    QScope *scopes;
    uint32_t nscopes;
};

enum { QSEM_INSTS = 64 }; /* a generic's instances at most: past it, one calls itself growing */

extern const char *const qsem_ends[];
/* qsem_chain.c: each chain step's and end's name to fn with ctx. */
void qsem_chain_methods(QsemMethodFn *fn, void *ctx);

/* qsem_say.c */
void qsem_say(QSem *S, const QTok *t, ...);
const char *qsem_text(QSem *S, const QTok *t);
const char *qsem_tname(QType t);
const char *qsem_join(QSem *S, const char *a, const char *b, const char *c);
const char *qsem_uint(QSem *S, uint32_t n);
const char *qsem_aname(QSem *S, QType t);

/* qsem_scope.c */
bool qsem_same(const QTok *a, const QTok *b);
bool qsem_is(const QTok *t, const char *word);
void qsem_push(QSem *S);
void qsem_pop(QSem *S);
uint32_t qsem_modof(const QSem *S, const QTok *t);
QBind *qsem_scoped(QSem *S, QScope *sc, QBind b);
void qsem_bind(QSem *S, const QTok *name, QVar *var, QFn *fn);
uint32_t qsem_floor(const QSem *S);
const QBind *qsem_usable(QSem *S, const QBind *b, const QTok *name);
const QBind *qsem_exported(const QSem *S, uint32_t k, const QTok *name);
const QBind *qsem_member(QSem *S, const QBind *m, const QTok *name);
const QBind *qsem_find(QSem *S, const QTok *name);
const QBind *qsem_qual(QSem *S, const QExpr *e);
bool qsem_ismod(QSem *S, const QExpr *e);
const QBind *qsem_lookup(QSem *S, const QExpr *e);
void qsem_bindty(QSem *S, const QTok *name, QType t);
QVar *qsem_var(QSem *S, const QTok *name, QType type, QVarKind kind);
QVar *qsem_root(const QExpr *e);
uint32_t qsem_at(const QSem *S, const QTok *t);
/* Where a use or a move at t is: t's place, or an inlined body's call's. */
uint32_t qsem_useat(const QSem *S, const QTok *t);

/* qsem_type.c */
QType qsem_scalar(const QTok *t);
bool qsem_owns(const QSem *S, QType t);
QType qsem_newty(QSem *S, QTyKind kind);
/* elem's array of dims dimensions, counts each one's, null a dynamic one: list[elem] when it grows;
   interned. */
QType qsem_shapedof(QSem *S, QType elem, uint8_t dims, const uint32_t *counts, bool grows);
/* Can an array at at have n dimensions? Else it says why. */
bool qsem_dimsfit(QSem *S, const QTok *at, uint32_t n);
/* Does a T[] parameter of type param take an array of type t, another, by its descriptor: a
   T[n, m] of its elements? */
bool qsem_lendable(const QSem *S, QType t, QType param);
/* elem's array of count, 0 a dynamic one, of one dimension: qsem_shapedof's. */
QType qsem_arrayof(QSem *S, QType elem, uint32_t count, bool grows);
/* The type e names where a value is written: Name, m.Name, int, Name[T]; QY_VOID none. */
QType qsem_typevalue(QSem *S, const QExpr *e);
QType qsem_tupleof(QSem *S, const QType *items, uint32_t n);
QType qsem_wrapof(QSem *S, QTyKind kind, QType val);
QType qsem_fnof(QSem *S, const QType *params, uint32_t n, QType result);
bool qsem_wraps(const QSem *S, QType t, QTyKind kind);
QType qsem_inner(const QSem *S, QType t);
bool qsem_isgen(const QSem *S, QType t);
bool qsem_model(const QSem *S, QType t);
bool qsem_ismap(const QSem *S, QType t);
QType qsem_type(QSem *S, const QTypeEx *x);
QExpr *qsem_const(QSem *S, const QTok *tok, QType type, QVal v);
QExpr *qsem_bool(QSem *S, const QTok *tok, bool b);
bool qsem_isconst(const QExpr *e);
bool qsem_inrange(int64_t v, bool big, QType t);
int64_t qsem_trunc(int64_t v, QType t);
double qsem_num(const QExpr *e);
const QPiece *qsem_cat(QSem *S, const QPiece *a, const QPiece *b);
bool qsem_widens(QType from, QType to);
bool qsem_fits(const QExpr *e, QType to);
QType qsem_meet(const QExpr *a, const QExpr *b);
QExpr *qsem_fold_conv(QSem *S, QExpr *e, QType to);
QExpr *qsem_settle(QSem *S, QExpr *e);
QExpr *qsem_conv(QSem *S, QExpr *a, QType to);
void qsem_mismatch(QSem *S, const QExpr *e, QType to);
QExpr *qsem_coerce(QSem *S, QExpr *e, QType to);
QType qsem_sumof(QType t);

/* qsem_expr.c */
bool qsem_plain(QSem *S, const QExpr *e, uint32_t count);
QExpr *qsem_print(QSem *S, QExpr *e);
QExpr *qsem_convert(QSem *S, QExpr *e, QType to);
QExpr *qsem_wrapped_ops(QSem *S, QExpr *e);
QExpr *qsem_mutarg(QSem *S, QExpr *v, const QParam *p);
QExpr *qsem_expr(QSem *S, QExpr *e, QType hint);

/* qsem_opt.c */
QExpr *qsem_part(QSem *S, QExpr *a, uint32_t k);
QExpr *qsem_error(QSem *S, QExpr *e, QType hint);
QExpr *qsem_isnone(QSem *S, QExpr *e);
QExpr *qsem_coalesce(QSem *S, QExpr *e);
bool qsem_istext(const QFn *f);
QExpr *qsem_spell(QSem *S, QExpr *v, const char *who);
QExpr *qsem_try(QSem *S, QExpr *e);
QExpr *qsem_get(QSem *S, QExpr *e);

/* qsem_move.c */
void qsem_dropcalls(QSem *S, QType t);
void qsem_callee(QSem *S, const QExpr *e);
void qsem_lends(QSem *S);
void qsem_lent(QSem *S, const QExpr *e, const QFn *f);
const QArg **qsem_given(
    QSem *S,
    const QArg *args,
    const QTok *const *names,
    uint32_t n,
    const QTok *who,
    const char *what
);
QVar *qsem_rawroot(QSem *S, const QExpr *e);
const QLoan *qsem_loan(const QSem *S, const QVar *v);
bool qsem_changes(QSem *S, const QExpr *e, const QTok *at);
QExpr *qsem_owned(QSem *S, QExpr *e, bool returned);
void qsem_moved(QSem *S);
bool qsem_any(const QExpr *e, bool (*const hit)(const QExpr *, const QVar *), const QVar *v);
bool qsem_calls(const QExpr *e, const QVar *const);
bool qsem_byplace(const QSem *S, QType t);
QExpr *qsem_into(QSem *S, const QVar *v, QExpr *e);
QExpr *qsem_intoplace(QSem *S, const QExpr *t, QExpr *e);
QExpr *qsem_held(QSem *S, QExpr *e);
QExpr *qsem_addressed(QSem *S, QExpr *e);
QExpr *qsem_wrapped(QSem *S, const QTok *tok, QType t, bool flag, QExpr *val);
QExpr *qsem_wrap(QSem *S, QType to, QExpr *v);
QExpr *qsem_value(QSem *S, QExpr *e, QType to);

/* qsem_rec.c */
QExpr *qsem_build(
    QSem *S,
    QExpr *e,
    QType t,
    const QField *fields,
    uint32_t n,
    const QTok *who,
    bool all
);
QExpr *qsem_variant(QSem *S, QExpr *e, QType t, bool call);
bool qsem_isrec(const QSem *S, QType t);
bool qsem_private(QSem *S, QType t, const QTok *name, bool priv, const QTok *at);
QExpr *qsem_field(QSem *S, QExpr *e);
QExpr *qsem_slice(QSem *S, QExpr *e);
QExpr *qsem_index(QSem *S, QExpr *e);
QExpr *qsem_tuple(QSem *S, QExpr *e, QType hint);

/* qsem_list.c */
QExpr *qsem_keysof(QSem *S, QExpr *m);
QExpr *qsem_len(QSem *S, QExpr *e);
QExpr *qsem_copy(QSem *S, QExpr *e);
QFn *qsem_methodof(const QSem *S, QType t, const QTok *name);
bool qsem_owntype(const QSem *S, QType t);
bool qsem_named(QSem *S, const QFn *f, const QTok *t);
QType qsem_peek(QSem *S, const QExpr *e);
QType qsem_typeat(QSem *S, const QExpr *e);
QExpr *qsem_method(QSem *S, QExpr *e);
QExpr *qsem_has(QSem *S, QExpr *e);

/* qsem_call.c */
bool qsem_cheap(const QExpr *v);
QExpr *qsem_inline(QSem *S, QExpr *e, QFn *f, QExpr **argv);
QFn *qsem_operfor(const QSem *S, QKind op, const QExpr *a, const QExpr *b, bool *two);
QExpr *qsem_operator(QSem *S, QExpr *e, QKind op, QExpr *a, QExpr *b);
bool qsem_hasrel(const QSem *S, QKind op, const QExpr *a, const QExpr *b);
QExpr *qsem_relation(QSem *S, QExpr *e, QKind op, QExpr *a, QExpr *b);
QExpr *qsem_icall(QSem *S, QExpr *e, QExpr *f);
QExpr *qsem_fnref(QSem *S, QExpr *e, QFn *f);
QExpr *qsem_call(QSem *S, QExpr *e);
QExpr *qsem_called(QSem *S, QExpr *e, const QBind *b);
QExpr *qsem_name(QSem *S, QExpr *e);

/* qsem_generic.c */
void qsem_bindtypes(QSem *S, const QTy *y);
void qsem_typeargs(QSem *S, const QFn *f);
bool qsem_unify(
    QSem *S,
    const QTok *const *tp,
    uint32_t ntp,
    const QTypeEx *x,
    QType t,
    QType *targs
);
bool qsem_literal(
    QSem *S,
    const QTok *const *tp,
    uint32_t ntp,
    const QTypeEx *x,
    const QExpr *a,
    QType *targs,
    bool *bylit
);
bool qsem_lambda(QSem *S, QExpr *e, QType *t, bool open);
const QParam *qsem_paramof(const QSem *S, const QVar *v);
void qsem_hiddenargs(QSem *S, QExpr *e, uint32_t at, const QLam *l, const QExpr *a);
QFn *qsem_instance(QSem *S, QExpr *e, QFn *g);
bool qsem_takesfn(const QFn *f);
QExpr *qsem_fncall(QSem *S, QExpr *e, QFn *f);

/* qsem_chain.c */
bool qsem_method_of(QSem *S, const QExpr *e, const char *const *names, uint32_t n, uint32_t *k);
bool qsem_step(QSem *S, const QExpr *e, uint32_t *k);
QExpr *qsem_dup(QSem *S, const QExpr *e);
QExpr *qsem_at_index(QSem *S, QExpr *a, QVar *i);
QExpr *qsem_applied(
    QSem *S,
    const QIter *it,
    QExpr *a,
    QExpr *const *extra,
    uint32_t n,
    QType hint
);
QExpr *qsem_iter(QSem *S, QExpr *e, uint32_t k, QStmt *loop);
void qsem_chainfor(QSem *S, QStmt *s);

/* qsem_generator.c */
void qsem_fnbody(QSem *S, QFn *f);
QFn *qsem_genof(QSem *S, const QExpr *e);
void qsem_yield(QSem *S, QStmt *s);
void qsem_genfor(QSem *S, QStmt *s, QFn *g);
void qsem_kept(QSem *S, QDecl *d, QKind kw, QFn *g);
QExpr *qsem_state(QSem *S, QExpr *call, QFn *g);
void qsem_keep(QSem *S, QFn *r);
QExpr *qsem_next(QSem *S, QExpr *e, QVar *v);

/* qsem_match.c */
void qsem_match(QSem *S, QStmt *s);
void qsem_case(QSem *S, QStmt *s);
void qsem_endmatch(QSem *S, const QStmt *s);

/* qsem_stmt.c */
void qsem_dropix(QSem *S, QType t);
void qsem_decls(QSem *S, QStmt *s);
QExpr *qsem_nameof(QSem *S, QVar *v, const QTok *tok);
void qsem_lend(QSem *S, const QVar *root, const QStmt *s);
void qsem_stmt(QSem *S, QStmt *s);

/* qsem_decl.c */
void qsem_consts(QSem *S, const QMod *m);
void qsem_types(QSem *S, const bool *in);
void qsem_typed(QSem *S, QType t);
void qsem_signature(QSem *S, QFn *f);
bool qsem_isdrop(const QFn *f);
void qsem_dropin(QSem *S, QType t, const QTok *name);
void qsem_dropok(QSem *S, QType t);
void qsem_fndef(QSem *S, QFn *f);
bool qsem_inmodel(const QSem *S, const QFn *f);
QType qsem_typeinst(QSem *S, QType g, const QType *targs, const QTok *at);
QType qsem_given_types(QSem *S, QType g, QTypeEx *const *items, uint32_t n, const QTok *at);

/* qsem_mod.c */
void qsem_module(QSem *S, uint32_t k);
void qsem_newscopes(QSem *S);
void qsem_scopes(QSem *S, const bool *in);
void qsem_exports(QSem *S);

#endif
