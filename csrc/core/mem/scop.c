#include "mem/scop.h"
#include "comp/blok.h"
#include "comp/comp.h"
#include "io/repl.h"
#include "mem/symb.h"

Scop scop = {.nbnd = 1, .depth = 1};

static const Binding none; /* a binding of nothing */

static uint32_t hash(const uint16_t scope, const uint16_t name) { return (scope * 7u + name) & (SCHASH - 1); }

void scop_init(void)
{
    scop.nbnd = 1;
    memset(scop.heads, 0, sizeof scop.heads);
    memset(scop.lnbit, 0, sizeof scop.lnbit);
}

Binding *scop_get(const uint16_t scope, const uint16_t name)
{
    for (uint16_t i = scop.heads[hash(scope, name)]; i; i = scop.bnd[i].next)
        if (scop.bnd[i].scope == scope && scop.bnd[i].name == name)
            return &scop.bnd[i];
    return nullptr;
}

Binding *scop_bind(const uint16_t scope, const uint16_t name)
{
    Binding *const found = scop_get(scope, name);
    if (found)
        return found;
    if (scop.nbnd >= NBND)
        repl_error(E_OM);
    const uint32_t h = hash(scope, name);
    Binding *const b = &scop.bnd[scop.nbnd];
    *b = (Binding){.scope = scope, .name = name, .next = scop.heads[h]};
    scop.heads[h] = (uint16_t)scop.nbnd++;
    return b;
}

/* Push procedure proc's scope; none for 0. */
static void enter(const uint8_t proc)
{
    if (!proc)
        return;
    if (scop.depth >= NSCS)
        repl_error(E_OM);
    scop.stack[scop.depth++] = proc;
}

void scop_proc(const uint8_t proc)
{
    comp.proc = proc;
    scop.depth = 1;
    scop.stack[0] = 0;
    enter(proc);
}

/* Stack entry i's scope: its procedure's name, 0 the module; *deffn a DEF FN's, which sees all
   of the scope under it. */
static uint16_t frame(const uint32_t i, bool *const deffn)
{
    const uint16_t p = scop.stack[i];
    *deffn = false;
    if (!p)
        return 0;
    *deffn = blok.procs[p].flags & PF_DEF;
    return blok.procs[p].name;
}

uint16_t scop_own(void)
{
    uint32_t i = scop.depth;
    bool deffn;
    uint16_t sc;
    do
        sc = frame(--i, &deffn);
    while (deffn && i);
    return sc;
}

const Binding *scop_look(const uint16_t name, uint16_t *const scope)
{
    uint32_t i = scop.depth;
    bool shared_only = false; /* only what the scope above sees */
    bool deffn;
    do {
        uint16_t sc = frame(--i, &deffn);
        const Binding *b = scop_get(sc, name);
        if (b && b->flags != BF_USE && (!shared_only || (b->flags & BF_SHR))) {
            if ((b->flags & BF_REF) && i) { /* SHARED: the scope under it's, declared or not */
                sc = frame(--i, &deffn);
                b = scop_get(sc, name);
                if (!b)
                    b = &none;
            }
            *scope = sc;
            return b;
        }
        shared_only = !deffn; /* under it, seen only when shared, unless a DEF FN's */
    } while (i);
    *scope = scop_own();
    return &none;
}

/* Name's binding as the procedure compiled sees it. */
static const Binding *seen(const uint16_t name)
{
    uint16_t scope;
    return scop_look(name, &scope);
}

uint8_t scop_type(const uint16_t name, const bool array) { return seen(name)->et[array]; }
uint8_t scop_rec(const uint16_t name, const bool array) { return seen(name)->rec[array]; }
uint16_t scop_fix(const uint16_t name, const bool array) { return seen(name)->fix[array]; }

uint8_t scop_sfx(const uint16_t name, const uint8_t type, const bool array)
{
    const Binding *const b = seen(name);
    if (b->flags & BF_CON)
        return b->et[0];
    const uint8_t et = b->et[array];
    if (b->rec[array] || (et && et != type))
        repl_error(E_RD);
    return type;
}

bool scop_con(const uint16_t name) { return seen(name)->flags & BF_CON; }

bool scop_known(const uint16_t name) { return seen(name) != &none; }

bool scop_mshr(const uint16_t name)
{
    const Binding *const b = scop_get(0, name);
    return b && (b->flags & (BF_SHR | BF_CON)) == BF_SHR;
}

/* Has a scope label key: a name, a line number when scope has SC_NUM? */
static bool lany(const uint16_t scope, const uint16_t key)
{
    if (scope & SC_NUM)
        return scop.lnbit[key / WORD_BITS] >> (key % WORD_BITS) & 1;
    return symb_name(key)->flags & NM_LBL;
}

void scop_label(const uint16_t scope, const uint16_t key, const uint32_t line)
{
    if (lany(scope, key))
        repl_error(E_DL);
    if (scope & SC_NUM)
        scop.lnbit[key / WORD_BITS] |= 1u << (key % WORD_BITS);
    else
        symb_name(key)->flags |= NM_LBL;
    Binding *const b = scop_bind(scope, key);
    b->flags |= BF_LBL;
    b->line = line;
}

uint32_t scop_lfind(const uint16_t scope, const uint16_t key)
{
    const Binding *const b = scop_get(scope, key);
    if (b && (b->flags & BF_LBL))
        return b->line;
    if (lany(scope, key))
        repl_error(E_SPE);
    return 0;
}

