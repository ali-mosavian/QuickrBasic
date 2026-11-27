/* Quickr's semantic pass: names resolved, types checked, conversions made explicit, constants
 * folded.
 *
 * # How it works
 * Names live on a stack of bindings, a block's scope a mark into it. A module's own names and
 * those its imports bind are its scope's: a name not on the stack is looked up in the scope of
 * the module its token is written in, so a generic's or a lambda's body sees its own module's
 * wherever it is checked. m.x is x of m's scope, if public; from m import X binds an alias, X
 * found in m's scope when used. The passes:
 *   1. each module's imports; each fn's and type's name; then each module's consts, in order;
 *      then each type's fields and each fn's signature, their defaults folded with those consts
 *      in sight;
 *   2. each module's statements, a module after those it imports, fn bodies skipped: a var or
 *      let is seen from its line on;
 *   3. each fn's body, its module's names (its fns, consts, vars and lets) in sight;
 *   4. each from's names checked public, each move against the uses after it.
 *
 * This file runs the passes; each part of the checking is a file of its own, behind qsem_int.h:
 * names (qsem_scope), types (qsem_type), expressions (qsem_expr and one file each for calls,
 * generics, records, lists, optionals, chains), statements (qsem_stmt, qsem_match,
 * qsem_generator), ownership (qsem_move), declarations (qsem_decl), modules (qsem_mod) and the
 * errors' text (qsem_say).
 *
 * A compound type is an entry of QProg.tys; lists, arrays and tuples are interned, so a type is
 * the same as another when its number is. A record, tuple or list lives in a place: a value of
 * one is built straight into the place it goes to, or first in a temporary when it reads that
 * place, or is no place's and its address is wanted. A for's element and a case's datum name
 * the place they stand for (QVar.place), so they cost nothing. A for over a list borrows it till
 * next: no change to it but through its element.
 *
 * An expression gets its type bottom up. A literal is untyped until its place types it: a
 * binary operator's other side, a declaration's as, a parameter's type; alone, an int or a
 * double. Two values meet in one type, widening where nothing is lost (int to double, single to
 * double; int and single meet in double). / of ints is a double. An expression of constants
 * folds to one, so a const, a default and a for's step are values here.
 *
 * Every error is reported and the pass goes on; QY_ERROR marks what it spoiled, so it is said
 * once.
 *
 * # Example
 *    var r as single = 6     6: untyped, typed single by the as
 *    let d = r * 2.5         2.5 fits a single: r * 2.5 is a single
 *    let h = 7 / 2           3.5: an untyped double, a double by the let
 */
#include "qr/qsem_int.h"

/* A statement of kind of the run's own, at tok. */
static QStmt *qsem_made(QSem *const S, const QStmtKind kind, const QTok *const tok)
{
    QStmt *const s = qast_alloc(S->mem, sizeof *s);
    s->kind = kind;
    s->tok = tok;
    s->own = &S->prog->own;
    return s;
}

/* --test: each test run, a module's after those it imports; a test's error handled by its
   QS_TESTFAIL; the tally said at the end. */
static void qsem_tests(QSem *const S)
{
    QProg *const p = S->prog;

    if (!p->testing)
        return;

    for (uint32_t i = 0; i < p->nmods; i++) {
        const QMod *const m = &p->mods[p->order[i]];

        for (QStmt *s = m->head; s; s = qsem_nextin(m, s)) {
            if (s->kind != QS_FN || !s->fn->test || !s->fn->sig)
                continue;

            QStmt *const run = qsem_made(S, QS_TESTRUN, s->tok);
            run->fn = s->fn;
            run->link = s->fn->onfail = qsem_made(S, QS_TESTFAIL, s->tok);
            run->link->fn = s->fn;
            p->runs = qast_room(S->mem, p->runs, p->nruns, &p->capruns, sizeof *p->runs);
            p->runs[p->nruns++] = run;
        }
    }

    p->testend = qsem_made(S, QS_TESTEND, p->nruns ? p->runs[0]->tok : p->mods[0].toks);
    p->failed = qsem_var(S, p->testend->tok, QY_INT, QV_TEMP);
    p->passes = qsem_var(S, p->testend->tok, QY_INT, QV_TEMP);
    p->fails = qsem_var(S, p->testend->tok, QY_INT, QV_TEMP);
}

/* The program's end, when a module var owns a value with drop: each dropped there. */
static void qsem_atend(QSem *const S)
{
    if (!S->prog->own.nowned)
        return;

    QStmt *const s = S->prog->atend = qast_alloc(S->mem, sizeof *s);
    s->kind = QS_ATEND;
    s->tok = S->prog->own.owned[0]->name;
    s->own = &S->prog->own;
}

/* Fn f's body, its parameters in its scope: once, and a generic record's method once called. */
static void qsem_body(QSem *const S, QFn *const f)
{
    if (f->checked || f->idle || !f->end || !f->sig || f->native || qast_generic(f) || f->lamof ||
        qast_inline(f))
        return;

    f->checked = true;

    qsem_fnbody(S, f);

    if (f->resumable)
        qsem_keep(S, f);

    S->fn = nullptr;
}

/* Is fn f the modules in's, by where it is written? */
static bool qsem_in(const QSem *const S, const bool *const in, const QFn *const f)
{
    return in[qsem_modof(S, f->name)];
}

QSem *qsem_start(QProg *const p, QMem *const m)
{
    QSem *const S = qast_alloc(m, sizeof *S);
    *S = (QSem){.prog = p, .mem = m};

    qsem_newscopes(S);
    return S;
}

void qsem_check(QSem *const S, const bool *const in)
{
    QProg *const p = S->prog;
    qsem_scopes(S, in);

    for (QFn *f = p->fns; f; f = f->next)
        if (qsem_in(S, in, f) && !f->lamof && !f->op && !f->test && !f->in && !qsem_isdrop(f)) {
            S->pub = !f->priv;
            qsem_bind(S, f->name, nullptr, f);
            S->pub = false;
        }

    qsem_types(S, in);

    for (uint32_t i = 0; i < p->nmods; i++)
        if (in[p->order[i]])
            qsem_consts(S, &p->mods[p->order[i]]);

    for (uint32_t i = 0; i < p->tys.n; i++) /* those of a stage before typed already */
        qsem_typed(S, QY_FIRST + i);

    for (QFn *f = p->fns; f; f = f->next) /* a generator's too, its instances' model */
        if (qsem_in(S, in, f) && !f->ntp && !f->lamof && !qsem_inmodel(S, f))
            qsem_signature(S, f);

    for (QFn *f = p->fns; f; f = f->next)
        if (qsem_in(S, in, f) && !qsem_inmodel(S, f))
            qsem_fndef(S, f);

    for (uint32_t i = 0; i < p->nmods; i++) {
        if (!in[p->order[i]])
            continue;

        qsem_module(S, p->order[i]);

        while (S->nmarks)
            qsem_pop(S);
    }

    for (uint32_t checked = 1; checked;) { /* again while a body calls a method not checked */
        checked = 0;

        for (QFn *f = p->fns; f; f = f->next) { /* a later stage's have no signature yet */
            const bool was = f->checked;
            qsem_body(S, f);
            checked += f->checked && !was;
        }
    }
}

void qsem_finish(QSem *const S)
{
    QProg *const p = S->prog;

    for (uint32_t i = 0; i < p->tys.n; i++)
        qsem_dropok(S, QY_FIRST + i);

    qsem_lends(S);

    qsem_exports(S);
    qsem_moved(S);
    qsem_atend(S);
    qsem_tests(S);

    if (!p->keep)
        return;

    p->sem = qast_alloc(S->mem, sizeof *p->sem);
    *p->sem = (QSemKeep){.scopes = S->scopes, .nscopes = S->nscopes};
}

bool qsem_probe(QProg *const p, QMem *const m, const QScopeAt *const at, QStmt *const s)
{
    const uint32_t errs = p->errs.n;
    QSem S = {
        .prog = p,
        .mem = m,
        .scopes = p->sem->scopes,
        .nscopes = p->sem->nscopes,
    };

    for (S.lastvar = p->vars; S.lastvar && S.lastvar->next; S.lastvar = S.lastvar->next)
        ;

    qsem_push(&S);

    if (at) { /* as they were: a block's name over an outer one's */
        S.binds = at->binds;
        S.nbinds = S.capbinds = at->n;
    }

    S.fn = at ? at->fn : nullptr;
    qsem_push(&S);

    if (s->kind == QS_CALL || s->kind == QS_ASSIGN)
        qsem_stmt(&S, s);
    else
        qsem_say(&S, s->tok, "only a call or an assignment runs here", nullptr);

    return p->errs.n == errs;
}
