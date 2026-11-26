/* Quickr's semantic pass: modules: imports, packages, and a QBasic module's exports.
 *
 * # How it works
 * Each module's scope binds its imports; a QBasic module's TYPEs and procedures are bound in
 * its own scope as Quickr's, why one is not usable kept to say where it is used.
 */
#include "qr/qsem_int.h"
#include "comp/export.h"

/* May module k's top level have s: the main's anything, another's declarations alone? */
static bool qsem_declares(const uint32_t k, const QStmt *const s)
{
    switch (s->kind) {
    case QS_DECL:
    case QS_TYPE:
    case QS_FIELD:
    case QS_ENDTYPE:
    case QS_ENUM:
    case QS_VARIANT:
    case QS_ENDENUM:
    case QS_IMPORT:
    case QS_FROM:
    case QS_FN:
        return true;

    default:
        return !k;
    }
}

/* Module k's statements, fn bodies and early consts skipped; then its vars are all there. */
void qsem_module(QSem *const S, const uint32_t k)
{
    const QMod *const m = &S->prog->mods[k];

    for (QStmt *s = m->head; s; s = qsem_nextin(m, s)) {
        if (s->kind == QS_FN || s->early)
            continue;

        if (!qsem_declares(k, s)) {
            qsem_say(
                S,
                s->tok,
                "a module runs nothing: its top level has function, type, enum, const, var, let "
                "and "
                "import",
                nullptr
            );
            continue;
        }

        S->pub = s->kind == QS_DECL && !s->priv && !S->nmarks;
        qsem_stmt(S, s);
        S->pub = false;
    }

    S->scopes[k].ready = true;
}

/* A new scope, a package folder's when mod is QMOD_NONE: its number + 1, a binding's scope. */
static uint32_t qsem_newscope(QSem *const S, const uint32_t mod)
{
    S->scopes = qast_room(S->mem, S->scopes, S->nscopes, &S->capscopes, sizeof *S->scopes);
    S->scopes[S->nscopes] = (QScope){.mod = mod};

    return ++S->nscopes;
}

/* In scope k, the package folder name names, made the first time: its scope + 1. */
static uint32_t qsem_package(QSem *const S, const uint32_t k, const QTok *const name)
{
    const QBind *const b = qsem_exported(S, k, name);

    if (b && b->scope && S->scopes[b->scope - 1].mod == QMOD_NONE)
        return b->scope;

    const uint32_t sc = qsem_newscope(S, QMOD_NONE);
    qsem_scoped(S, &S->scopes[k], (QBind){.name = name, .scope = sc});

    return sc;
}

/* import im in module k: its name bound to it, a.b's a a package's folder holding b. */
static void qsem_import(QSem *const S, const uint32_t k, const QImport *const im)
{
    const QBind spoilt = {.name = im->as ? im->as : im->path[0], .ty = QY_ERROR};

    const QBind b = {.name = im->as ? im->as : im->path[im->npath - 1], .scope = im->mod + 1};

    if (im->mod == QMOD_NONE) { /* the loader said */
        qsem_scoped(S, &S->scopes[k], spoilt);
        return;
    }

    if (im->as || im->npath == 1) {
        qsem_scoped(S, &S->scopes[k], b);
        return;
    }

    uint32_t in = k;

    for (uint32_t i = 0; i + 1 < im->npath; i++)
        in = qsem_package(S, in, im->path[i]) - 1;

    if (!qsem_exported(S, in, b.name))
        qsem_scoped(S, &S->scopes[in], b);
}

/* from m import X [as Y], ..: each Y an alias of m's X, found when used; a package folder's
   names its modules. */
static void qsem_from(QSem *const S, const uint32_t k, const QStmt *const s)
{
    for (const QImport *im = s->names; im; im = im->next) {
        const QTok *const name = im->as ? im->as : im->tok;
        QBind b = {.name = name, .ty = QY_ERROR};

        if (s->imps->folder && im->mod != QMOD_NONE)
            b = (QBind){.name = name, .scope = im->mod + 1};
        else if (!s->imps->folder && s->imps->mod != QMOD_NONE)
            b = (QBind){.name = name, .scope = s->imps->mod + 1, .alias = im->tok};

        qsem_scoped(S, &S->scopes[k], b);
    }
}

/* ---- a QBasic module's exports (comp/export.h) */

/* A QBasic name: text's len bytes, matched case aside. */
static const QTok *qsem_qbtok(QSem *const S, const char *const text, const uint32_t len)
{
    QTok *const t = qast_alloc(S->mem, sizeof *t);
    *t = (QTok){
        .kind = QK_NAME,
        .line = UINT32_MAX,
        .len = len,
        .text = text,
        .nocase = true,
    };

    return t;
}

/* The record of QBasic TYPE td, QY_ERROR none. */
static QType qsem_qbrec(const QSem *const S, const uint32_t td)
{
    for (uint32_t i = 0; i < S->prog->tys.n; i++)
        if (S->prog->tys.at[i].foreign && S->prog->tys.at[i].td == td)
            return QY_FIRST + i;

    return QY_ERROR;
}

/* An export's type et as Quickr's: QY_VOID none, QY_ERROR a TYPE no module Quickr imports gives. */
static QType qsem_qbtype(const QSem *const S, const uint32_t et)
{
    switch (et) {
    case ET_IMP:
        return QY_VOID;

    case ET_I2:
    case ET_I4:
        return QY_INT;

    case ET_SH:
        return QY_SHORT;

    case ET_BY:
        return QY_UBYTE;

    case ET_R4:
        return QY_SINGLE;

    case ET_R8:
        return QY_DOUBLE;

    case ET_SD:
        return QY_STRING;

    default:
        return et >= ET_REC ? qsem_qbrec(S, et - ET_REC) : QY_ERROR;
    }
}

/* b bound in QBasic module k's scope, public: it and one there of its name, case aside, both an
   error where used. */
static void qsem_qbbind(QSem *const S, const uint32_t k, QBind b)
{
    static const char twice[] = "two of its module's names match it, case aside";

    QScope *const sc = &S->scopes[k];
    b.pub = true;

    for (uint32_t i = 0; i < sc->n; i++)
        if (qsem_same(sc->at[i].name, b.name))
            sc->at[i].why = b.why = twice;

    sc->at = qast_room(S->mem, sc->at, sc->n, &sc->cap, sizeof *sc->at);
    sc->at[sc->n++] = b;
}

/* QBasic module k's TYPEs, records with no fields yet. */
static void qsem_qbrecords(QSem *const S, const uint32_t k)
{
    const Exports *const x = S->prog->mods[k].ex;

    for (uint32_t i = 0; i < x->ntypes; i++) {
        const ExpType *const e = &x->types[i];
        const QType t = qsem_newty(S, QT_REC);
        QTy *const y = qast_ty(S->prog, t);
        y->name = qsem_qbtok(S, e->name, e->len);
        y->foreign = y->laid = true;
        y->td = e->td;
        y->size = e->size;
        y->deep = e->deep;
    }
}

/* QBasic module k's TYPEs' fields, its procedures functions, its CONSTs consts: each bound in its
   scope, why one is not usable kept. */
static void qsem_qbnames(QSem *const S, const uint32_t k)
{
    static const char unseen[] = "a TYPE no module it imports exports";

    const Exports *const x = S->prog->mods[k].ex;

    for (uint32_t i = 0; i < x->ntypes; i++) {
        const ExpType *const e = &x->types[i];
        const QType t = qsem_qbrec(S, e->td);
        QTy *const y = qast_ty(S->prog, t);
        const char *why = e->why;
        y->fields = qast_alloc(S->mem, (e->nfields ? e->nfields : 1) * sizeof *y->fields);
        y->nfields = y->capfields = e->nfields;

        for (uint32_t f = 0; f < e->nfields; f++) {
            const ExpField *const ef = &e->fields[f];
            const QType et = qsem_qbtype(S, ef->et);
            const QType ft =
                (ef->list || ef->count) && et != QY_ERROR ? qsem_listof(S, et, ef->count) : et;
            why = why || ft != QY_ERROR ? why : unseen;
            y->fields[f] = (QField){
                .name = qsem_qbtok(S, ef->name, ef->len),
                .type = ft,
                .off = ef->off,
            };
        }

        qsem_qbbind(S, k, (QBind){.name = y->name, .ty = t, .why = why});
    }

    for (uint32_t i = 0; i < x->nprocs; i++) {
        const ExpProc *const e = &x->procs[i];
        QFn *const f = qast_alloc(S->mem, sizeof *f);
        const char *why = e->why;
        *f = (QFn){
            .name = qsem_qbtok(S, e->name, e->len),
            .npar = e->npar,
            .params = qast_alloc(S->mem, (e->npar ? e->npar : 1) * sizeof *f->params),
            .result = qsem_qbtype(S, e->result.et),
            .byarg = e->resarg,
            .sig = true,
            .checked = true,
            .foreign = true,
            .ncall = e->native,
            .proc = e->proc,
        };

        why = why || f->result != QY_ERROR ? why : unseen;

        for (uint32_t j = 0; j < e->npar; j++) {
            const ExpVal *const v = &e->params[j];
            const QTok *const name = qsem_qbtok(S, v->name, v->len);
            const QType et = qsem_qbtype(S, v->et);
            const QType pt = v->array && et != QY_ERROR ? qsem_listof(S, et, 0) : et;
            QVar *const var = qast_alloc(S->mem, sizeof *var);
            *var = (QVar){.name = name, .type = pt, .kind = QV_PARAM};

            f->params[j] = (QParam){.name = name, .var = var, .ref = v->byref || v->array};

            why = why || pt != QY_ERROR ? why : unseen;
        }

        f->next = S->prog->foreign;
        S->prog->foreign = f;
        qsem_qbbind(S, k, (QBind){.name = f->name, .fn = f, .why = why});
    }

    for (uint32_t i = 0; i < x->nconsts; i++) { /* known: a const; else its CONST's variable */
        const ExpConst *const e = &x->consts[i];
        QVar *const v = qast_alloc(S->mem, sizeof *v);
        *v = (QVar){
            .name = qsem_qbtok(S, e->name, e->len),
            .type = qsem_qbtype(S, e->et),
            .kind = e->known ? QV_CONST : QV_LET,
            .val = {.i = e->i},
            .constant = e->known,
            .shared = true,
            .id = e->var,
        };

        qsem_qbbind(S, k, (QBind){.name = v->name, .var = v});
    }
}

/* Each module's scope, its imports bound; a QBasic module's its exports. */
void qsem_scopes(QSem *const S)
{
    for (uint32_t k = 0; k < S->prog->nmods; k++)
        qsem_newscope(S, k);

    for (uint32_t k = 0; k < S->prog->nmods; k++)
        if (S->prog->mods[k].ex)
            qsem_qbrecords(S, k);

    for (uint32_t k = 0; k < S->prog->nmods; k++)
        if (S->prog->mods[k].ex)
            qsem_qbnames(S, k);

    for (uint32_t k = 0; k < S->prog->nmods; k++) {
        const QMod *const m = &S->prog->mods[k];

        for (QStmt *s = m->head; s; s = qsem_nextin(m, s)) {
            for (const QImport *im = s->kind == QS_IMPORT ? s->imps : nullptr; im; im = im->next)
                qsem_import(S, k, im);

            if (s->kind == QS_FROM)
                qsem_from(S, k, s);
        }
    }
}

/* Each from's names: an error where one its module does not export is. */
void qsem_exports(QSem *const S)
{
    for (uint32_t k = 0; k < S->prog->nmods; k++) {
        const QMod *const m = &S->prog->mods[k];

        for (QStmt *s = m->head; s; s = qsem_nextin(m, s)) {
            const QImport *const from = s->kind == QS_FROM ? s->imps : nullptr;

            if (!from || from->folder || from->mod == QMOD_NONE)
                continue;

            const QBind mod = {.name = from->path[from->npath - 1], .scope = from->mod + 1};

            for (const QImport *im = s->names; im; im = im->next)
                qsem_member(S, &mod, im->tok);
        }
    }
}
