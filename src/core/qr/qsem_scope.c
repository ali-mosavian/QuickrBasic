/* Quickr's semantic pass: names and their scopes, and what a lambda captures.
 *
 * # How it works
 * Names live on a stack of bindings, a block's scope a mark into it; a module's own names and
 * those its imports bind are its scope's (qsem.c). A lambda's name of an outer fn's storage is
 * a capture, a hidden parameter of its own. A place's variable is its root.
 */
#include "qr/qsem_int.h"

static const QBind *qsem_capture(QSem *S, QLamAt *c, uint32_t i, const QTok *use);

/* ---- names */

bool qsem_same(const QTok *const a, const QTok *const b)
{
    if (a->len != b->len)
        return false;
    if (!a->nocase && !b->nocase)
        return !memcmp(a->text, b->text, a->len);

    for (uint32_t i = 0; i < a->len; i++)
        if (downcase((uint8_t)a->text[i]) != downcase((uint8_t)b->text[i]))
            return false;

    return true;
}

bool qsem_is(const QTok *const t, const char *const word)
{
    return t->len == strlen(word) && !memcmp(t->text, word, t->len);
}

void qsem_push(QSem *const S)
{
    S->marks = qast_room(S->mem, S->marks, S->nmarks, &S->capmarks, sizeof *S->marks);
    S->marks[S->nmarks++] = S->nbinds;
}

void qsem_pop(QSem *const S)
{
    if (S->nmarks)
        S->nbinds = S->marks[--S->nmarks];
}

/* The module token t is written in. */
uint32_t qsem_modof(const QSem *const S, const QTok *const t)
{
    return t->line < S->prog->src.nlines ? S->prog->src.lines[t->line].mod : 0;
}

/* b bound in scope sc; an error if its name already is. */
QBind *qsem_scoped(QSem *const S, QScope *const sc, const QBind b)
{
    for (uint32_t i = 0; i < sc->n; i++)
        if (qsem_same(sc->at[i].name, b.name)) {
            qsem_say(S, b.name, qsem_text(S, b.name), " is already declared", nullptr);
            return nullptr;
        }

    sc->at = qast_room(S->mem, sc->at, sc->n, &sc->cap, sizeof *sc->at);
    sc->at[sc->n] = b;
    return &sc->at[sc->n++];
}

/* b bound in the innermost scope, a module's own outside blocks and fns (a public statement's
   exported); an error if its name already is. */
static QBind *qsem_bound_as(QSem *const S, QBind b)
{
    if (!S->nmarks) {
        b.pub = S->pub;
        return qsem_scoped(S, &S->scopes[qsem_modof(S, b.name)], b);
    }

    for (uint32_t i = S->marks[S->nmarks - 1]; i < S->nbinds; i++)
        if (qsem_same(S->binds[i].name, b.name)) {
            qsem_say(S, b.name, qsem_text(S, b.name), " is already declared", nullptr);
            return nullptr;
        }

    S->binds = qast_room(S->mem, S->binds, S->nbinds, &S->capbinds, sizeof *S->binds);
    S->binds[S->nbinds] = b;
    return &S->binds[S->nbinds++];
}

void qsem_bind(QSem *const S, const QTok *const name, QVar *const var, QFn *const fn)
{
    qsem_bound_as(S, (QBind){.name = name, .var = var, .fn = fn});
}

/* The bindings every scope sees: the module's own, below its first block's or fn's. */
uint32_t qsem_floor(const QSem *const S) { return S->nmarks ? S->marks[0] : S->nbinds; }

/* What names nothing, said already. */
static const QBind qsem_spoilt = {.ty = QY_ERROR};

/* b, a module's name, used at name: an error if it is a QBasic one Quickr can't use. */
const QBind *qsem_usable(QSem *const S, const QBind *const b, const QTok *const name)
{
    if (!b || !b->why)
        return b;

    qsem_say(S, name, qsem_text(S, name), " is not usable from Quickr: ", b->why, nullptr);
    return &qsem_spoilt;
}

/* name in module k's scope, an alias followed; null none. */
static const QBind *qsem_global(QSem *const S, const uint32_t k, const QTok *const name)
{
    const QScope *const sc = &S->scopes[k];

    for (uint32_t i = sc->n; i-- > 0;) {
        const QBind *const b = &sc->at[i];

        if (!qsem_same(b->name, name))
            continue;
        if (!b->alias)
            return b;

        const QBind *const x = qsem_exported(S, b->scope - 1, b->alias);

        if (x && x->pub)
            return qsem_usable(S, x, name);
        if (!x && !S->scopes[b->scope - 1].ready)
            qsem_say(
                S,
                name,
                qsem_text(S, name),
                " is not there yet: its module's vars start after this one's",
                nullptr
            );

        return &qsem_spoilt; /* else said at its import */
    }

    return nullptr;
}

/* name among scope k's own, not its imports; null none. A package folder's are its modules,
   public each. */
const QBind *qsem_exported(const QSem *const S, const uint32_t k, const QTok *const name)
{
    const QScope *const sc = &S->scopes[k];

    for (uint32_t i = 0; i < sc->n; i++)
        if (qsem_same(sc->at[i].name, name) && !sc->at[i].alias)
            return &sc->at[i];

    return nullptr;
}

/* name in module m (a module's binding): its public binding, else an error said. In a module
   whose statements are not checked yet, a var is not there. */
const QBind *qsem_member(QSem *const S, const QBind *const m, const QTok *const name)
{
    const QBind *const b = qsem_exported(S, m->scope - 1, name);
    const QScope *const sc = &S->scopes[m->scope - 1];

    if (b && (b->pub || sc->mod == QMOD_NONE))
        return b;

    if (b) {
        qsem_say(S, name, qsem_text(S, name), " is not public in ", qsem_text(S, m->name), nullptr);
        return &qsem_spoilt;
    }

    if (sc->ready || sc->mod == QMOD_NONE)
        qsem_say(S, name, qsem_text(S, m->name), " has no ", qsem_text(S, name), nullptr);
    else
        qsem_say(
            S,
            name,
            qsem_text(S, name),
            " is not there yet: its module's vars start after this one's",
            nullptr
        );

    return &qsem_spoilt;
}

const QBind *qsem_find(QSem *const S, const QTok *const name)
{
    for (uint32_t i = S->nbinds; i-- > 0;)
        if ((i < S->hidelo || i >= S->hidehi) && qsem_same(S->binds[i].name, name)) {
            QLamAt *const c = S->lamat;

            if (!c || i >= c->mark || i < qsem_floor(S))
                return &S->binds[i];

            return qsem_capture(S, c, i, name);
        }

    return qsem_global(S, qsem_modof(S, name), name);
}

/* Unchecked e as a name: a name's binding, or m.x's, m a module or a package's: a.b.x. Null
   when e is neither, or names nothing. */
const QBind *qsem_qual(QSem *const S, const QExpr *const e)
{
    if (e->kind == QE_NAME && !e->a)
        return qsem_find(S, e->tok);
    if (e->kind != QE_FIELD && e->kind != QE_NAME)
        return nullptr;

    const QBind *const m = qsem_qual(S, e->a);
    return m && m->scope && !m->alias ? qsem_usable(S, qsem_member(S, m, e->tok), e->tok) : nullptr;
}

/* Is unchecked e a module's name, or a package's? */
bool qsem_ismod(QSem *const S, const QExpr *const e)
{
    const QBind *const b = e->kind == QE_NAME || e->kind == QE_FIELD ? qsem_qual(S, e) : nullptr;
    return b && b->scope && !b->alias;
}

/* What unchecked name e binds: a name's, m.x's (e->a m) once qsem_field made it a name. */
const QBind *qsem_lookup(QSem *const S, const QExpr *const e)
{
    return e->kind == QE_NAME ? qsem_qual(S, e) : nullptr;
}

void qsem_bindty(QSem *const S, const QTok *const name, const QType t)
{
    qsem_bound_as(S, (QBind){.name = name, .ty = t});
}

QVar *qsem_var(QSem *const S, const QTok *const name, const QType type, const QVarKind kind)
{
    QVar *const v = qast_alloc(S->mem, sizeof *v);
    *v = (QVar){
        .name = name,
        .type = type,
        .kind = kind,
        .fn = S->fn,
    };

    if (S->lastvar)
        S->lastvar->next = v;
    else
        S->prog->vars = v;

    S->lastvar = v;
    return v;
}

/* ---- captures */

/* The kind a capture's hidden parameter takes from v, what it stands for: changeable as v is. */
static QVarKind qsem_capkind(const QVar *const v)
{
    if (v->kind == QV_VAR || v->kind == QV_MUT || (v->kind == QV_EACH && v->mut))
        return QV_MUT;

    return v->kind == QV_EACH ? QV_LET : v->kind;
}

/* Binding i, outside lambda c's scope, as c's body sees it: as it is when no fn's storage (a
   module's, a const, a fn, a type); else c's hidden parameter for it, made on first use. A lambda
   c is in sees it first. The original's last use is use. */
static const QBind *qsem_capture(
    QSem *const S,
    QLamAt *const c,
    const uint32_t i,
    const QTok *const use
)
{
    QLam *const l = c->lam;

    for (uint32_t k = 0; k < l->ncaps; k++)
        if (l->caps[k]->at == i)
            return c->views[k];

    const QBind *const outer =
        c->up && i < c->up->mark ? qsem_capture(S, c->up, i, use) : &S->binds[i];
    QVar *const v = outer->var;

    if (S->binds[i].var) {
        QVar *const orig = S->binds[i].var;
        QVar *const root = orig->place ? qsem_root(orig->place) : orig;
        const uint32_t at = qsem_at(S, use);
        root->last = at > root->last ? at : root->last;
    }

    QBind *const view = qast_alloc(S->mem, sizeof *view);
    *view = *outer;

    if (v && v->lam) { /* a lambda bound to a parameter: no value to reach */
        qsem_say(
            S,
            use,
            qsem_text(S, use),
            " is a lambda: call it outside another lambda",
            nullptr
        );
        *view = (QBind){.name = outer->name, .ty = QY_ERROR};

        return view;
    }

    QCap *const cap = qast_alloc(S->mem, sizeof *cap);
    *cap = (QCap){
        .name = S->binds[i].name,
        .var = v,
        .fn = outer->fn,
        .ty = outer->ty,
        .at = i,
        .frame = v && !v->constant && v->fn,
    };

    if (cap->frame) {
        cap->param = qsem_var(S, cap->name, v->type, qsem_capkind(v));
        cap->param->fn = l->fn;
        cap->param->cap = cap;
        view->var = cap->param;
    }

    l->caps = qast_room(S->mem, l->caps, l->ncaps, &l->capcaps, sizeof *l->caps);
    c->views = qast_room(S->mem, c->views, l->ncaps, &c->capviews, sizeof *c->views);
    c->views[l->ncaps] = view;
    l->caps[l->ncaps++] = cap;
    return view;
}

/* A checked place's variable: its name's, its fields' and elements' base's; null none. */
QVar *qsem_root(const QExpr *e)
{
    while (e->kind == QE_FIELD || e->kind == QE_INDEX)
        e = e->a;

    return e->kind == QE_NAME ? e->var : nullptr;
}

uint32_t qsem_at(const QSem *const S, const QTok *const t)
{
    const QMod *const m = &S->prog->mods[qsem_modof(S, t)];
    return m->base + (uint32_t)(t - m->toks);
}
