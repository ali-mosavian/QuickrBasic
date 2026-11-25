/* What an IMPORT or FROM binds in the module that has it.
 *
 * # How it works
 * Each module has its own names (Name.module). A name that stands for another module's is an
 * alias: symb_alias holds the exporter's name. Binding one copies what the declarations and the
 * compiler read: a procedure's flags, a TYPE's number; for a CONST or a variable, a binding in the
 * module's scope, BF_IMP, with the exporter's type, which comp_scope follows to the exporter's
 * variable. Link gives each alias its procedure (bind_procs).
 *
 * A module exports its SUBs, FUNCTIONs, CONSTs and TYPEs but the PRIVATE ones, and its DIM PUBLIC
 * variables; never a name it imports. IMPORT gfx binds gfx (NM_MOD): each name of the module that
 * starts `gfx.` stands for gfx's export up to the next dot, or is Name not exported where used
 * (NM_BAD). A * binding yields to the module's own declaration, before or after it, and to a listed
 * name; one two *'s bind is Duplicate definition where used.
 *
 * COMMON joins the program's files' variables by position (comp/declare.c): a later module's name
 * for one stands for the first's, BF_IMP as an import's, BF_CMN its own still.
 */
#include "comp/bind.h"

[[noreturn]]
static void fail(Fault *const f, const uint8_t code)
{
    fault_error(f, code);
}

/* Name n's binding in its module's scope, null none. */
static Binding *module_binding(const Scop *const sc, const Symb *const sy, const uint32_t n)
{
    return scop_get(sc, sc_module(symb_name(sy, n)->module), n);
}

uint8_t bind_exports(const Scop *const sc, const Symb *const sy, const uint32_t t)
{
    if (!t || *symb_alias(sy, t) || symb_name(sy, t)->flags & (NM_PRIV | NM_MOD | NM_BAD))
        return 0;

    const Binding *const b = module_binding(sc, sy, t);
    const uint16_t flags = b ? b->flags : 0;
    return (flags & BF_PROC ? BIND_PROC : 0) | (symb_name(sy, t)->rec ? BIND_TYPE : 0) |
           (flags & BF_CON ? BIND_CONST : 0) | (flags & BF_PUB ? BIND_VAR : 0);
}

/* Name n declared by its own module: DIM, CONST, SUB, FUNCTION, TYPE. */
static bool declares(const Scop *const sc, const Symb *const sy, const uint32_t n)
{
    if (*symb_alias(sy, n))
        return false;

    const Binding *const b = module_binding(sc, sy, n);
    return symb_name(sy, n)->rec || (b && b->flags != BF_USE);
}

/* Name n an error where used: code. */
static void bad(Symb *const sy, const uint32_t n, const uint8_t code)
{
    Name *const nm = symb_name(sy, n);
    nm->flags = (uint8_t)((nm->flags & ~(NM_SUB | NM_FUNC)) | NM_BAD);
    nm->rec = 0;
    *symb_alias(sy, n) = code;
}

/* Name n of module m stands for t, its kinds k; star: a * binding. */
static void bind(
    Scop *const sc,
    Symb *const sy,
    const uint32_t n,
    const uint32_t t,
    const uint8_t k,
    const bool star
)
{
    *symb_alias(sy, n) = t | (star ? BIND_STAR : 0);
    Name *const nm = symb_name(sy, n);
    const Name *const tn = symb_name(sy, t);

    if (k & BIND_PROC)
        nm->flags |= tn->flags & (NM_SUB | NM_FUNC);
    if (k & BIND_TYPE)
        nm->rec = tn->rec;
    if (!(k & (BIND_CONST | BIND_VAR)))
        return;

    const uint32_t from = sc_module(tn->module), to = sc_module(nm->module);
    const Binding *const e = scop_get(sc, from, t);
    Binding *const b = scop_bind(sc, to, n);
    b->flags = (uint16_t)((e->flags & (BF_CON | BF_VAL)) | BF_SHR | BF_IMP);
    memcpy(b->et, e->et, sizeof b->et);
    memcpy(b->rec, e->rec, sizeof b->rec);
    memcpy(b->fix, e->fix, sizeof b->fix);
    b->val = e->val;
    nm->flags |= tn->flags & NM_REC;

    if (scop_get(sc, from | SC_ARR, t))
        scop_bind(sc, to | SC_ARR, n);
}

/* The name n binds in module m: its AS name, else its text. */
static uint32_t bound_name(Symb *const sy, const uint32_t m, const ImptName *const n)
{
    return n->aslen ? symb_find_name(sy, m, n->as, n->aslen)
                    : symb_find_name(sy, m, n->text, n->len);
}

/* Name n about to be bound by a listed name or IMPORT: Duplicate definition, declared or bound. */
static void fresh(const Scop *const sc, const Symb *const sy, Fault *const f, const uint32_t n)
{
    const uint32_t a = *symb_alias(sy, n);
    const bool listed = a && !(a & BIND_STAR) && !(symb_name(sy, n)->flags & NM_BAD);

    if (listed || symb_name(sy, n)->flags & NM_MOD || declares(sc, sy, n))
        fail(f, E_RD);
}

/* Name n, `prefix.e..`, prefix a module's name its module binds: that module's export e, through
   the name `prefix.e`. */
static void bind_dotted(Scop *const sc, Symb *const sy, const uint32_t n, const uint32_t prefix)
{
    uint8_t text[UINT8_MAX]; /* the table may move as it grows */
    const uint8_t *const t = symb_text(sy, n);
    const uint8_t all = t[0], len = symb_text(sy, prefix)[0];
    memcpy(text, t + 1 + all, all);

    const uint8_t *const dot = memchr(text + len + 1, '.', all - len - 1u);
    const uint8_t end = dot ? (uint8_t)(dot - text) : all;
    const uint32_t to = *symb_alias(sy, prefix);
    const uint32_t e = symb_look_name(sy, to, text + len + 1, (uint8_t)(end - len - 1));
    const uint8_t k = bind_exports(sc, sy, e);
    const uint32_t whole = symb_find_name(sy, symb_name(sy, n)->module, text, end);

    if (!k)
        bad(sy, whole, E_NEX);
    else
        bind(sc, sy, whole, e, k, false);

    if (whole != n &&
        !(k & BIND_VAR && symb_name(sy, e)->flags & NM_REC)) /* .f: a record's field */
        bad(sy, n, E_NEX);
}

void bind_module(
    Scop *const sc,
    Symb *const sy,
    Fault *const f,
    const uint32_t m,
    const ImptName *const n,
    const uint32_t to
)
{
    const uint32_t name = bound_name(sy, m, n);

    if (symb_name(sy, name)->flags & NM_MOD && *symb_alias(sy, name) == to) /* imported again */
        return;

    fresh(sc, sy, f, name);
    symb_name(sy, name)->flags |= NM_MOD;
    *symb_alias(sy, name) = to;

    const uint8_t len = symb_text(sy, name)[0];
    const uint32_t count = sy->nnames; /* those bind_dotted makes are bound */

    for (uint32_t i = 1; i < count; i++) {
        const uint8_t *const t = symb_text(sy, i);

        if (symb_name(sy, i)->module == m && t[0] > len + 1 && t[1 + len] == '.' &&
            !memcmp(t + 1, symb_text(sy, name) + 1, len))
            bind_dotted(sc, sy, i, name);
    }
}

void bind_name(
    Scop *const sc,
    Symb *const sy,
    Fault *const f,
    const uint32_t m,
    const uint32_t from,
    const ImptName *const n
)
{
    const uint32_t t = symb_look_name(sy, from, n->text, n->len);
    const uint8_t k = bind_exports(sc, sy, t);

    if (!k || (n->arr && !(k & BIND_VAR && scop_get(sc, sc_module(from) | SC_ARR, t))))
        fail(f, E_NEX);
    if (n->sfx && k & (BIND_CONST | BIND_VAR) &&
        module_binding(sc, sy, t)->et[n->arr] != comp_suffix(n->sfx)) /* as where it is used */
        fail(f, E_RD);

    const uint32_t name = bound_name(sy, m, n);
    fresh(sc, sy, f, name);
    symb_name(sy, name)->flags &= (uint8_t)~NM_BAD;
    bind(sc, sy, name, t, k, false);
}

void bind_each_export(
    const Scop *const sc,
    const Symb *const sy,
    const uint32_t m,
    const uint8_t kinds,
    BindExportFn *const fn,
    void *const ctx
)
{
    const uint32_t count = sy->nnames; /* those fn makes aside */

    for (uint32_t t = 1; t < count; t++) {
        const uint8_t k = symb_name(sy, t)->module == m ? bind_exports(sc, sy, t) & kinds : 0;

        if (k)
            fn(ctx, t, k);
    }
}

/* bind_star's: module m, binding. */
typedef struct {
    Scop *sc;
    Symb *sy;
    uint32_t m;
} Star;

/* bind_star's: export t, its kinds k, bound in the module unless it names t otherwise. */
static void star(void *const ctx, const uint32_t t, const uint8_t k)
{
    const Star *const s = ctx;
    const uint32_t n = symb_name_in(s->sy, s->m, t), a = *symb_alias(s->sy, n);

    if (declares(s->sc, s->sy, n) || symb_name(s->sy, n)->flags & (NM_MOD | NM_BAD) ||
        (a && !(a & BIND_STAR)))
        return;
    if (a && a != (t | BIND_STAR)) /* another module's *'s */
        bad(s->sy, n, E_RD);
    else
        bind(s->sc, s->sy, n, t, k, true);
}

void bind_star(Scop *const sc, Symb *const sy, const uint32_t m, const uint32_t from)
{
    Star s = {sc, sy, m};

    if (from != m)
        bind_each_export(sc, sy, from, (uint8_t)~BIND_VAR, star, &s);
}

void bind_common(Scop *const sc, Symb *const sy, const uint32_t n, const uint32_t to)
{
    *symb_alias(sy, n) = to;
    module_binding(sc, sy, n)->flags |= BF_IMP | BF_CMN;
}

bool bind_own(Scop *const sc, Symb *const sy, const uint32_t n)
{
    Name *const nm = symb_name(sy, n);
    uint32_t *const a = symb_alias(sy, n);
    Binding *const b = module_binding(sc, sy, n);

    if (b && b->flags & BF_CMN) /* COMMON's, its own still: DIM gives its array's size */
        return true;
    if (nm->flags & NM_MOD || (*a && !(*a & BIND_STAR) && !(nm->flags & NM_BAD)))
        return false;
    if (!*a)
        return true;
    if (b && b->flags & BF_IMP)
        *b = (Binding){.scope = b->scope, .name = b->name, .next = b->next};

    nm->flags &= (uint8_t)~(NM_SUB | NM_FUNC | NM_BAD);
    nm->rec = 0;
    *a = 0;
    return true;
}

void bind_use(const Symb *const sy, Fault *const f, const uint32_t n)
{
    const uint8_t flags = symb_name(sy, n)->flags;

    if (flags & NM_BAD)
        fail(f, (uint8_t)*symb_alias(sy, n));
    if (flags & NM_MOD)
        fail(f, E_RD);
}

void bind_procs(Symb *const sy)
{
    for (uint32_t n = 1; n < sy->nnames; n++) {
        const uint32_t a = *symb_alias(sy, n);

        if (a && !(symb_name(sy, n)->flags & (NM_MOD | NM_BAD)))
            symb_name(sy, n)->proc = symb_name(sy, a & ~BIND_STAR)->proc;
    }
}
