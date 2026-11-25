/* Quickr's semantic pass: declarations: consts, types, fields and fn signatures.
 *
 * # How it works
 * Pass 1 (qsem.c): each module's consts in order, each type's fields and each fn's signature,
 * their defaults folded; a generic record's instance made the first time it is named.
 */
#include "qr/qsem_int.h"
#include "qr/qnat.h"
#include "qr/qpar.h"

static void qsem_native(QSem *S, QFn *f);

/* ---- the passes */

/* Module m's statement after s, past a fn's: null after its last. */
QStmt *qsem_nextin(const QMod *const m, QStmt *s)
{
    if (s->kind == QS_FN && !s->fn->end)
        return nullptr;
    if (s->kind == QS_FN)
        s = s->fn->end;

    return s == m->tail ? nullptr : s->next;
}

/* Module m's consts, outside blocks and fns, in order. */
void qsem_consts(QSem *const S, const QMod *const m)
{
    uint32_t depth = 0;

    for (QStmt *s = m->head; s; s = qsem_nextin(m, s)) {
        if (s->kind == QS_FN)
            continue;

        const bool opens = (s->kind == QS_IF && !s->oneline) || s->kind == QS_WHILE ||
                           s->kind == QS_DO || s->kind == QS_FOR || s->kind == QS_SELECT;
        const bool closes = s->kind == QS_ENDIF || s->kind == QS_ENDWHILE || s->kind == QS_ENDDO ||
                            s->kind == QS_ENDFOR || s->kind == QS_ENDSELECT;

        depth += opens;
        depth -= closes && depth;

        if (!depth && s->kind == QS_DECL && s->op == QK_CONST) {
            s->early = S->early = true;
            S->pub = s->pub;
            qsem_decls(S, s);
            S->early = S->pub = false;
        }
    }
}

/* Is name taken in type y's one set of names, its fields', values' and methods': said. */
static bool qsem_taken(QSem *const S, const QTy *const y, const QTok *const name)
{
    const char *what = nullptr;

    for (uint32_t k = 0; k < y->nfields && y->kind == QT_REC; k++)
        what = qsem_same(y->fields[k].name, name) ? " is already a field" : what;
    for (uint32_t k = 0; k < y->nvars; k++)
        what = qsem_same(y->vars[k].name, name) ? " is already a value" : what;
    for (uint32_t k = 0; k < y->nmeths; k++)
        what = qsem_same(y->meths[k]->name, name) ? " is already a method" : what;

    if (what)
        qsem_say(S, name, qsem_text(S, name), what, nullptr);

    return what;
}

/* A field of type t declared: d's name, type and default. */
static void qsem_addfield(QSem *const S, const QType t, QDecl *const d)
{
    QTy *const y = qast_ty(S->prog, t);
    qsem_taken(S, y, d->name);
    y->fields = qast_room(S->mem, y->fields, y->nfields, &y->capfields, sizeof *y->fields);
    y->fields[y->nfields++] = (QField){.name = d->name, .tx = d->type, .def = d->value};
}

/* An enum's value: its name, then its data's fields. */
static void qsem_addvariant(QSem *const S, const QType t, const QDecl *const d)
{
    QTy *const y = qast_ty(S->prog, t);
    qsem_taken(S, y, d->name);
    y->vars = qast_room(S->mem, y->vars, y->nvars, &y->capvars, sizeof *y->vars);
    QVariant *const v = &y->vars[y->nvars++];
    *v = (QVariant){.name = d->name};

    for (const QDecl *x = d->next; x; x = x->next)
        v->n++;

    v->fields = qast_alloc(S->mem, (v->n ? v->n : 1) * sizeof *v->fields);
    uint32_t k = 0;

    for (const QDecl *x = d->next; x; x = x->next)
        v->fields[k++] = (QField){.name = x->name, .tx = x->type, .def = x->value};

    y->data |= v->n > 0;
}

/* Type or enum statement s's members, type t's: its fields or values, then its methods. */
static void qsem_members(QSem *const S, QStmt *const s, const QType t)
{
    for (QStmt *m = s->next; m && m->kind != QS_ENDTYPE && m->kind != QS_ENDENUM; m = m->next)
        if (m->kind == QS_FIELD)
            for (QDecl *d = m->decls; d; d = d->next) {
                qsem_addfield(S, t, d);
                qast_ty(S->prog, t)->fields[qast_ty(S->prog, t)->nfields - 1].priv = m->priv;
            }
        else if (m->kind == QS_VARIANT)
            qsem_addvariant(S, t, m->decls);
        else if (m->kind == QS_FN && m->fn->in == s) {
            QTy *const y = qast_ty(S->prog, t);

            if (!qsem_taken(S, y, m->fn->name)) {
                y->meths = qast_room(S->mem, y->meths, y->nmeths, &y->capmeths, sizeof *y->meths);
                y->meths[y->nmeths++] = m->fn;
            }

            m = m->fn->end ? m->fn->end : m;
        }
}

/* Each type and enum: a type, its name bound, its fields and values declared; a generic record's
   at each instance. */
void qsem_types(QSem *const S)
{
    for (QStmt *s = S->prog->first; s; s = s->next) {
        if (s->kind != QS_TYPE && s->kind != QS_ENUM)
            continue;

        const QType t = s->ty = qsem_newty(S, s->kind == QS_TYPE ? QT_REC : QT_ENUM);
        QTy *const y = qast_ty(S->prog, t);
        y->name = s->decls->name;
        y->def = s;
        y->state = 0;
        y->tparams = s->tparams;
        y->ntp = s->kind == QS_TYPE ? s->ntp : 0; /* an enum's, said */
        S->pub = s->pub;
        qsem_bindty(S, s->decls->name, t);
        S->pub = false;

        if (!qast_ty(S->prog, t)->ntp)
            qsem_members(S, s, t);

        if (s->kind == QS_ENUM && !qast_ty(S->prog, t)->nvars)
            qsem_say(S, s->tok, "an enum needs a value", nullptr);
    }
}

/* Field f of type t typed: its type, else its default's; a record by value typed first, never
   t itself. */
static void qsem_fieldtype(QSem *const S, const QType t, QField *const f)
{
    QType ft = f->tx ? qsem_type(S, f->tx) : QY_VOID;

    if (f->def) {
        QExpr *d = qsem_expr(S, f->def, ft);
        d = f->tx ? qsem_coerce(S, d, ft) : qsem_settle(S, d);

        if (!qsem_isconst(d) && d->type != QY_ERROR)
            qsem_say(S, d->tok, "a default is a constant", nullptr);
        if (!f->tx)
            ft = d->type;

        f->def = d;
    }

    if (!f->tx && !f->def) {
        qsem_say(S, f->name, qsem_text(S, f->name), " needs a type", nullptr);
        ft = QY_ERROR;
    }

    const QType inner = qast_compound(ft) && qast_ty(S->prog, ft)->kind == QT_ARRAY
                            ? qast_ty(S->prog, ft)->elem
                            : ft;

    if (qast_record(S->prog, inner) &&
        (!qast_compound(ft) || qast_ty(S->prog, ft)->kind != QT_LIST)) {
        qsem_typed(S, inner);

        if (qast_ty(S->prog, inner)->state == 1) {
            qsem_say(
                S,
                f->name,
                qsem_text(S, f->name),
                " holds its own type: hold it in a list",
                nullptr
            );
            ft = QY_ERROR;
        }
    }

    f->type = ft;
    qast_ty(S->prog, t)->owns |= qsem_owns(S, ft);
}

/* Type t's fields typed: an enum with data's its tag, then each value's data. */
void qsem_typed(QSem *const S, const QType t)
{
    QTy *const y = qast_ty(S->prog, t);

    if (y->state)
        return;

    y->state = 1;

    if (y->kind == QT_ENUM && y->data) {
        uint32_t n = 1;

        for (uint32_t k = 0; k < y->nvars; k++)
            n += y->vars[k].n;

        QField *const fields = qast_alloc(S->mem, n * sizeof *fields);
        fields[0] = (QField){.name = y->name, .type = QY_INT};

        n = 1;

        for (uint32_t k = 0; k < y->nvars; k++) {
            QVariant *const v = &y->vars[k];
            v->first = n;

            for (uint32_t i = 0; i < v->n; i++)
                fields[n++] = v->fields[i];

            v->fields = &fields[v->first];
        }

        y->fields = fields;
        y->nfields = y->capfields = n;
    }

    for (uint32_t k = 0; k < qast_ty(S->prog, t)->nfields; k++)
        if (k || qast_ty(S->prog, t)->kind == QT_REC)
            qsem_fieldtype(S, t, &qast_ty(S->prog, t)->fields[k]);

    qast_ty(S->prog, t)->state = 2;
}

/* Fn f's parameters' and result's types, its defaults folded. */
void qsem_signature(QSem *const S, QFn *const f)
{
    S->fn = f;
    bool defaults = false;

    for (uint32_t k = 0; k < f->npar; k++) {
        QParam *const p = &f->params[k];
        QType t = p->type ? qsem_type(S, p->type) : QY_VOID;

        if (f->self && !k)
            t = f->in->ty;

        if (p->def) {
            QExpr *d = qsem_expr(S, p->def, t);
            d = p->type ? qsem_coerce(S, d, t) : qsem_settle(S, d);

            if (!qsem_isconst(d) && d->type != QY_ERROR)
                qsem_say(S, d->tok, "a default is a constant", nullptr);
            if (p->mut)
                qsem_say(S, p->name, "a mut parameter has no default", nullptr);
            if (!p->type)
                t = d->type;

            p->def = d;
            defaults = true;
        } else if (defaults)
            qsem_say(
                S,
                p->name,
                qsem_text(S, p->name),
                " needs a default: one before it has one",
                nullptr
            );

        if (!p->type && !p->def && !(f->self && !k)) {
            if (p->name->kind != QK_SELF) /* else said */
                qsem_say(S, p->name, qsem_text(S, p->name), " needs a type", nullptr);

            t = QY_ERROR;
        }

        p->var = qsem_var(S, p->name, t, p->mut ? QV_MUT : QV_PARAM);
    }

    f->result = f->rtype ? qsem_type(S, f->rtype) : QY_VOID;
    f->ytype = f->ytx ? qsem_type(S, f->ytx) : QY_VOID;

    if (f->resumable) /* next()'s: the value yielded, none when done */
        f->result = f->ytype == QY_ERROR ? QY_ERROR : qsem_wrapof(S, QT_OPT, f->ytype);

    f->byarg = qsem_byplace(S, f->result);

    if (f->native)
        qsem_native(S, f);

    if (f->result != QY_VOID)
        f->res = qsem_var(S, f->name, f->result, QV_RESULT);

    f->sig = true;
    S->fn = nullptr;
}

/* Operator fn f checked and kept by each record among its values' types, one of them declared
   in its module. */
static void qsem_operdef(QSem *const S, QFn *const f)
{
    const char *const op = qast_spelling(f->op);
    const QType r = f->op == QK_EQ || f->op == QK_LT ? QY_BOOL : f->result;
    const bool plain =
        f->npar && !f->params[0].mut && !f->params[f->npar - 1].mut && !f->params[f->npar - 1].def;
    bool err = false, home = false;

    if (!plain || f->npar > 2 || (f->npar == 1 && f->op != QK_MINUS)) {
        qsem_say(S, f->name, "an operator takes two plain values, or - one", nullptr);
        return;
    }

    for (uint32_t k = 0; k < f->npar; k++) {
        const QType t = f->params[k].var->type;
        err |= t == QY_ERROR;
        home |= qsem_isrec(S, t) && qast_ty(S->prog, t)->def &&
                qsem_modof(S, qast_ty(S->prog, t)->def->tok) == qsem_modof(S, f->name);
    }

    if (err || f->result == QY_ERROR)
        return;

    if (!home) {
        qsem_say(S, f->name, "one of an operator's values is a record of its module", nullptr);
        return;
    }

    if (f->result != r || r == QY_VOID) {
        qsem_say(
            S,
            f->name,
            "operator ",
            op,
            r == QY_BOOL ? " gives a bool" : " gives a value",
            nullptr
        );
        return;
    }

    const QExpr a = {.type = f->params[0].var->type};

    const QExpr b = {.type = f->params[f->npar - 1].var->type};

    bool two;
    const QFn *const was = qsem_operfor(S, f->op, &a, f->npar == 2 ? &b : nullptr, &two);

    if (was && was->params[0].var->type == a.type &&
        (f->npar == 1 || was->params[1].var->type == b.type)) {
        qsem_say(S, f->name, "an operator ", op, " of these values is there already", nullptr);
        return;
    }

    for (uint32_t k = 0; k < f->npar; k++) {
        const QType t = f->params[k].var->type;

        if (!qsem_isrec(S, t) || (k && t == f->params[0].var->type))
            continue;

        QTy *const y = qast_ty(S->prog, t);
        y->opers = qast_room(S->mem, y->opers, y->nopers, &y->capopers, sizeof *y->opers);
        y->opers[y->nopers++] = f;
    }
}

/* Is f named drop: a method, or a function said to be one? */
bool qsem_isdrop(const QFn *const f)
{
    return !f->lamof && !f->of && !f->test && !f->op && qsem_is(f->name, "drop");
}

/* fn drop(mut self) in a type: its drop. */
static void qsem_dropdef(QSem *const S, QFn *const f)
{
    if (!f->in) {
        qsem_say(S, f->name, "drop is a method: fn drop(mut self) in its type", nullptr);
        return;
    }

    if (f->in->kind != QS_TYPE) {
        qsem_say(S, f->name, "an enum has no drop", nullptr);
        return;
    }

    if (f->ntp || f->ytx || f->npar != 1 || !f->self || !f->params[0].mut || f->rtype)
        qsem_say(S, f->name, "drop takes mut self alone and gives nothing", nullptr);
    else
        qast_ty(S->prog, f->in->ty)->drop = f;
}

/* Does a value of type t hold one of type of: a field's, an element's, T?'s value? */
static bool qsem_holds_ty(QSem *const S, const QType t, const QType of)
{
    if (!qast_compound(t))
        return false;

    QTy *const y = qast_ty(S->prog, t);

    if (y->seen)
        return false;

    const bool list = y->kind == QT_LIST || y->kind == QT_ARRAY;
    const bool wrap = y->kind == QT_OPT || y->kind == QT_RES;
    const uint32_t n = list || wrap ? 1 : y->kind == QT_REC || y->kind == QT_TUPLE ? y->nfields : 0;
    bool any = false;
    y->seen = true;

    for (uint32_t k = 0; !any && k < n; k++) {
        const QType p = list ? y->elem : wrap ? y->fields[QF_VAL].type : y->fields[k].type;
        any = p == of || qsem_holds_ty(S, p, of);
    }

    y->seen = false;
    return any;
}

/* Can a place of type t, named at name, hold a value with drop: not as a map's value, nor in a
   list of lists? */
void qsem_dropin(QSem *const S, const QType t, const QTok *const name)
{
    if (!qast_compound(t) || !qast_drops(S->prog, qast_ty(S->prog, t)->elem))
        return;

    if (qsem_ismap(S, t))
        qsem_say(S, name, "a dict's values can't have drop", nullptr);
    else if (qast_array(S->prog, t) && qast_array(S->prog, qast_ty(S->prog, t)->elem))
        qsem_say(S, name, "a list of lists can't hold a value with drop", nullptr);
}

/* Type t with drop: no record holding itself; no enum's datum, map's value, list of lists. */
void qsem_dropok(QSem *const S, const QType t)
{
    const QTy *const y = qast_ty(S->prog, t);

    if (y->kind == QT_REC && qast_drops(S->prog, t) && qsem_holds_ty(S, t, t))
        qsem_say(S, y->name, qsem_aname(S, t), " has drop: it can't hold itself", nullptr);

    for (uint32_t k = 0; y->kind == QT_ENUM && k < y->nfields; k++)
        if (qast_drops(S->prog, y->fields[k].type))
            qsem_say(S, y->fields[k].name, "an enum's data can't have drop", nullptr);

    for (uint32_t k = 0; y->kind == QT_REC && k < y->nfields; k++)
        qsem_dropin(S, y->fields[k].type, y->fields[k].name);
}

/* Fn f, its signature made, as what it is: an operator, a drop, a text. */
void qsem_fndef(QSem *const S, QFn *const f)
{
    if (f->op && (f->ntp || f->ytx))
        qsem_say(S, f->name, "an operator is not generic and yields nothing", nullptr);
    else if (f->op)
        qsem_operdef(S, f);
    else if (qsem_isdrop(f))
        qsem_dropdef(S, f);
    else if (f->in && f->sig && qsem_is(f->name, "text") && !qsem_istext(f))
        qsem_say(S, f->name, "text takes self alone and gives a string", nullptr);
}

/* Is f a generic record's method, its model's or an instance's: made with its instance? */
bool qsem_inmodel(const QSem *const S, const QFn *const f)
{
    return f->in && f->in->kind == QS_TYPE && (f->in->ntp || qast_ty(S->prog, f->in->ty)->of);
}

/* What hides the names in sight where an instance is named, so its types see their own module's
   alone: put back by qsem_unhide. */
typedef struct {
    uint32_t lo, hi;
    QFn *fn;
    QType inst;
} QHide;

static QHide qsem_hide(QSem *const S, const QType inst)
{
    const QHide h = {S->hidelo, S->hidehi, S->fn, S->inst};

    S->hidelo = 0;
    S->hidehi = S->nbinds;
    S->fn = nullptr;
    S->inst = inst;
    qsem_push(S);
    return h;
}

static void qsem_unhide(QSem *const S, const QHide h)
{
    qsem_pop(S);
    S->hidelo = h.lo;
    S->hidehi = h.hi;
    S->fn = h.fn;
    S->inst = h.inst;
}

/* Generic record g's instance of types targs, made the first time (named at at): its type block
   parsed again, its fields typed and its methods' signatures made with g's type parameters bound
   to targs. Its methods' bodies are checked as every fn's. QY_ERROR past QSEM_INSTS. */
QType qsem_typeinst(QSem *const S, const QType g, const QType *const targs, const QTok *at)
{
    const uint32_t ntp = qast_ty(S->prog, g)->ntp;
    uint32_t made = 0;

    for (uint32_t i = 0; i < S->prog->tys.n; i++) {
        const QTy *const y = &S->prog->tys.at[i];

        if (y->of == g && !memcmp(y->targs, targs, ntp * sizeof *targs))
            return QY_FIRST + i;

        made += y->of == g;
    }

    if (made == QSEM_INSTS) {
        S->inst = QY_VOID; /* its instance's name past reading */
        qsem_say(
            S,
            at,
            qsem_text(S, qast_ty(S->prog, g)->name),
            " has too many instances: does it hold itself with ever new types?",
            nullptr
        );
        return QY_ERROR;
    }

    QType *const keep = qast_alloc(S->mem, ntp * sizeof *keep);
    memcpy(keep, targs, ntp * sizeof *keep);
    QStmt *const s = qpar_typeagain(S->prog, S->mem, qast_ty(S->prog, g)->def);
    const QType t = s->ty = qsem_newty(S, QT_REC);
    const QTy *const m = qast_ty(S->prog, g);
    QTy *const y = qast_ty(S->prog, t);
    y->name = m->name;
    y->def = s;
    y->state = 0;
    y->tparams = m->tparams;
    y->ntp = ntp;
    y->of = g;
    y->targs = keep;
    s->ntp = 0;
    s->pub = m->def->pub;
    qsem_members(S, s, t);

    const QHide h = qsem_hide(S, t);
    qsem_bindtypes(S, qast_ty(S->prog, t));
    qsem_typed(S, t);

    for (uint32_t k = 0; k < qast_ty(S->prog, t)->nmeths; k++) {
        QFn *const f = qast_ty(S->prog, t)->meths[k];
        f->idle = true;

        if (!f->ntp)
            qsem_signature(S, f);

        S->fn = nullptr;
    }

    for (uint32_t k = 0; k < qast_ty(S->prog, t)->nmeths; k++)
        qsem_fndef(S, qast_ty(S->prog, t)->meths[k]);

    if (qast_ty(S->prog, t)->drop) /* called when an owner ends */
        qast_ty(S->prog, t)->drop->idle = false;

    qsem_dropok(S, t);
    qsem_unhide(S, h);
    return t;
}

/* Generic record g given the types as written items, n of them: its instance; QY_ERROR after an
   error. */
QType qsem_given_types(
    QSem *const S,
    const QType g,
    QTypeEx *const *const items,
    const uint32_t n,
    const QTok *const at
)
{
    const uint32_t ntp = qast_ty(S->prog, g)->ntp;

    if (n != ntp) {
        qsem_say(
            S,
            at,
            qsem_text(S, qast_ty(S->prog, g)->name),
            ntp == 1 ? " takes 1 type" : qsem_join(S, " takes ", qsem_uint(S, ntp), " types"),
            nullptr
        );
        return QY_ERROR;
    }

    QType *const targs = qast_alloc(S->mem, ntp * sizeof *targs);

    for (uint32_t k = 0; k < n; k++)
        if ((targs[k] = qsem_type(S, items[k])) == QY_ERROR)
            return QY_ERROR;

    return qsem_typeinst(S, g, targs, at);
}

/* A number, a bool or a string: a native's argument and result. */
static bool qsem_nativeok(const QType t)
{
    return qast_numeric(t) || t == QY_BOOL || t == QY_STRING || t == QY_ERROR;
}

/* Native fn f's entry, and its types: numbers, bools and strings, by value. */
static void qsem_native(QSem *const S, QFn *const f)
{
    f->nat = qnat_find(f->name->text, f->name->len);

    if (!f->nat)
        qsem_say(S, f->name, "no native entry is named ", qsem_text(S, f->name), nullptr);

    const bool plain = f->result == QY_VOID || f->result == QY_ERROR || qsem_nativeok(f->result);

    for (uint32_t k = 0; k < f->npar; k++) {
        const QParam *const p = &f->params[k];

        if (p->mut || !qsem_nativeok(p->var->type))
            qsem_say(
                S,
                p->name,
                "a native function takes numbers, bools and strings, not mut",
                nullptr
            );
    }

    if (!plain)
        qsem_say(S, f->rtype->tok, "a native function gives a number, a bool or a string", nullptr);
}
