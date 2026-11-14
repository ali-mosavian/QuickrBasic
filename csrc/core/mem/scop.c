#include "mem/scop.h"
#include "session.h"
#include "comp/blok.h"
#include "comp/comp.h"
#include "io/repl.h"
#include "mem/symb.h"


static const Binding none; /* a binding of nothing */

static uint32_t hash(const uint16_t scope, const uint16_t name) { return (scope * 7u + name) & (SCHASH - 1); }

void scop_init(Session *const S)
{
    S->scop.nbnd = 1;
    memset(S->scop.heads, 0, sizeof S->scop.heads);
    memset(S->scop.lnbit, 0, sizeof S->scop.lnbit);
}

Binding *scop_get(Session *const S, const uint16_t scope, const uint16_t name)
{
    for (uint16_t i = S->scop.heads[hash(scope, name)]; i; i = S->scop.bnd[i].next)
        if (S->scop.bnd[i].scope == scope && S->scop.bnd[i].name == name)
            return &S->scop.bnd[i];
    return nullptr;
}

Binding *scop_bind(Session *const S, const uint16_t scope, const uint16_t name)
{
    Binding *const found = scop_get(S, scope, name);
    if (found)
        return found;
    if (S->scop.nbnd >= NBND)
        repl_error(S, E_OM);
    const uint32_t h = hash(scope, name);
    Binding *const b = &S->scop.bnd[S->scop.nbnd];
    *b = (Binding){.scope = scope, .name = name, .next = S->scop.heads[h]};
    S->scop.heads[h] = (uint16_t)S->scop.nbnd++;
    return b;
}

/* Push procedure proc's scope; none for 0. */
static void enter(Session *const S, const uint8_t proc)
{
    if (!proc)
        return;
    if (S->scop.depth >= NSCS)
        repl_error(S, E_OM);
    S->scop.stack[S->scop.depth++] = proc;
}

void scop_proc(Session *const S, const uint8_t proc)
{
    S->comp.proc = proc;
    S->scop.depth = 1;
    S->scop.stack[0] = 0;
    enter(S, proc);
}

/* Stack entry i's scope: its procedure's name, 0 the module; *deffn a DEF FN's, which sees all
   of the scope under it. */
static uint16_t frame(Session *const S, const uint32_t i, bool *const deffn)
{
    const uint16_t p = S->scop.stack[i];
    *deffn = false;
    if (!p)
        return 0;
    *deffn = blok_proc(&S->blok, p)->flags & PF_DEF;
    return blok_proc(&S->blok, p)->name;
}

uint16_t scop_own(Session *const S)
{
    uint32_t i = S->scop.depth;
    bool deffn;
    uint16_t sc;
    do
        sc = frame(S, --i, &deffn);
    while (deffn && i);
    return sc;
}

const Binding *scop_look(Session *const S, const uint16_t name, uint16_t *const scope)
{
    uint32_t i = S->scop.depth;
    bool shared_only = false; /* only what the scope above sees */
    bool deffn;
    do {
        uint16_t sc = frame(S, --i, &deffn);
        const Binding *b = scop_get(S, sc, name);
        if (b && b->flags != BF_USE && (!shared_only || (b->flags & BF_SHR))) {
            if ((b->flags & BF_REF) && i) { /* SHARED: the scope under it's, declared or not */
                sc = frame(S, --i, &deffn);
                b = scop_get(S, sc, name);
                if (!b)
                    b = &none;
            }
            *scope = sc;
            return b;
        }
        shared_only = !deffn; /* under it, seen only when shared, unless a DEF FN's */
    } while (i);
    *scope = scop_own(S);
    return &none;
}

/* Name's binding as the procedure compiled sees it. */
static const Binding *seen(Session *const S, const uint16_t name)
{
    uint16_t scope;
    return scop_look(S, name, &scope);
}

uint8_t scop_type(Session *const S, const uint16_t name, const bool array) { return seen(S, name)->et[array]; }
uint8_t scop_rec(Session *const S, const uint16_t name, const bool array) { return seen(S, name)->rec[array]; }
uint16_t scop_fix(Session *const S, const uint16_t name, const bool array) { return seen(S, name)->fix[array]; }

uint8_t scop_sfx(Session *const S, const uint16_t name, const uint8_t type, const bool array)
{
    const Binding *const b = seen(S, name);
    if (b->flags & BF_CON)
        return b->et[0];
    const uint8_t et = b->et[array];
    if (b->rec[array] || (et && et != type))
        repl_error(S, E_RD);
    return type;
}

bool scop_con(Session *const S, const uint16_t name) { return seen(S, name)->flags & BF_CON; }

bool scop_known(Session *const S, const uint16_t name) { return seen(S, name) != &none; }

bool scop_mshr(Session *const S, const uint16_t name)
{
    const Binding *const b = scop_get(S, 0, name);
    return b && (b->flags & (BF_SHR | BF_CON)) == BF_SHR;
}

/* Has a scope label key: a name, a line number when scope has SC_NUM? */
static bool lany(Session *const S, const uint16_t scope, const uint16_t key)
{
    if (scope & SC_NUM)
        return S->scop.lnbit[key / WORD_BITS] >> (key % WORD_BITS) & 1;
    return symb_name(&S->symb, key)->flags & NM_LBL;
}

void scop_label(Session *const S, const uint16_t scope, const uint16_t key, const uint32_t line)
{
    if (lany(S, scope, key))
        repl_error(S, E_DL);
    if (scope & SC_NUM)
        S->scop.lnbit[key / WORD_BITS] |= 1u << (key % WORD_BITS);
    else
        symb_name(&S->symb, key)->flags |= NM_LBL;
    Binding *const b = scop_bind(S, scope, key);
    b->flags |= BF_LBL;
    b->line = line;
}

uint32_t scop_lfind(Session *const S, const uint16_t scope, const uint16_t key)
{
    const Binding *const b = scop_get(S, scope, key);
    if (b && (b->flags & BF_LBL))
        return b->line;
    if (lany(S, scope, key))
        repl_error(S, E_SPE);
    return 0;
}

