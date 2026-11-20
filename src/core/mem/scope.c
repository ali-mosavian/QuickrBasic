/* Scoped declarations: what each name means in the module and in each procedure.
 *
 * # How it works
 * A binding is a name's in one scope, the module's (0) or a procedure's (its name), holding the
 * declared type, record and STRING * n of the scalar and of the array, a label's line and BF_*
 * flags. A procedure's binding, any but BF_USE alone, makes the name its own there.
 *
 * A label is a binding too; a line number's is in its scope | SC_NUM, keyed by the number. The
 * compiler's scopes are a stack of procedures, the module at its bottom; a name is looked up from
 * the top down. From a SUB or FUNCTION the module's names are seen only when BF_SHR, or BF_REF
 * there; from a DEF FN all.
 *
 * # asm structure (retired/asm32/mem/scope.asm)
 * | asm | here | |
 * |---|---|---|
 * | B_SC B_NAM B_NXT B_FLG B_ET B_REC B_FIX B_LINE, BSZ | Binding | MIRROR |
 * | g_bnd, g_nbnd, g_bhead, g_lnbit, g_scs, g_scn | Scop.arena, nbnd, heads, lnbit, stack, depth | D5 |
 * | g_bzero | `none` | MIRROR; static |
 * | scp_init | scop_init, scop_reset | MIRROR |
 * | scp_get, scp_bind | scop_get, scop_bind | MIRROR; D1 |
 * | scp_proc | scop_enter | D2 |
 * | scp_own, scp_look | scop_own, scop_look | MIRROR; D1 |
 * | scp_decl | scop_type, scop_rec, scop_fix | D3 |
 * | scp_arec | scop_rec(name, true) | D3 |
 * | scp_sfx | scop_sfx | MIRROR |
 * | scp_con, scp_mshr | scop_con, scop_mshr | MIRROR; D1 |
 * | scp_known | scop_known | MIRROR; D4 |
 * | scp_label, scp_lfind | scop_label, scop_lfind | MIRROR |
 * | scp_enter, scp_leave, scp_frame, scp_hash, scp_lany | enter, frame, hash, lany | MIRROR; static |
 *
 * - D1. Return values replace registers and CF: nullptr for none, true for is.
 * - D2. scop_enter takes the procedure itself; the caller (comp_proc) sets comp's cproc.
 * - D3. One routine per field of the declaration, each taking `array`.
 * - D4. True when bound; the asm's CF is the opposite.
 * - D5. Bindings come from Scop's arena and chain by pointer; a frame keeps its procedure's
 *   name and DEF FN flag. Errors raise through Fault.
 */
#include "mem/scope.h"
#include "util/grow.h"
#include <string.h>

static const Binding none; /* a binding of nothing */

static uint32_t scop_hash(const uint16_t scope, const uint16_t name)
{
    return (scope * 7u + name) & (SCHASH - 1);
}

[[noreturn]]
static void scop_fail(const Scop *const sc, const uint8_t code)
{
    fault_error(sc->fault, code);
}

void scop_init(Scop *const sc, Fault *const f)
{
    arena_init(&sc->arena, 0x4000);
    sc->fault = f;
    scop_reset(sc);
}

void scop_reset(Scop *const sc)
{
    arena_reset(&sc->arena);
    sc->gen++;
    sc->nbnd = 1;
    sc->depth = 1;
    sc->stack[0] = (ScopFrame){};

    sc->heads = arena_alloc(&sc->arena, SCHASH * sizeof *sc->heads);

    if (!sc->heads)
        scop_fail(sc, E_OM);
    if (sc->lnbit)
        memset(sc->lnbit, 0, sc->nlnbit * sizeof *sc->lnbit);
}

Binding *scop_get(const Scop *const sc, const uint16_t scope, const uint16_t name)
{
    for (Binding *b = sc->heads[scop_hash(scope, name)]; b; b = b->next)
        if (b->scope == scope && b->name == name)
            return b;

    return nullptr;
}

Binding *scop_bind(Scop *const sc, const uint16_t scope, const uint16_t name)
{
    Binding *const found = scop_get(sc, scope, name);
    sc->gen++; /* scop_look's answers stale: a binding made, or its flags to be changed */

    if (found)
        return found;

    Binding *const b = sc->nbnd < NBND ? arena_alloc(&sc->arena, sizeof *b) : nullptr;

    if (!b)
        scop_fail(sc, E_OM);

    const uint32_t h = scop_hash(scope, name);
    *b = (Binding){.scope = scope, .name = name, .next = sc->heads[h]};

    sc->heads[h] = b;
    sc->nbnd++;
    return b;
}

void scop_enter(Scop *const sc, const Proc *const p)
{
    sc->depth = 1;
    sc->stack[0] = (ScopFrame){};

    if (p)
        sc->stack[sc->depth++] = (ScopFrame){.name = p->name, .deffn = p->flags & PF_DEF};
}

void scop_inside(Scop *const sc, const uint16_t name)
{
    sc->stack[sc->depth++] = (ScopFrame){.name = name};
}

uint16_t scop_own(const Scop *const sc)
{
    uint32_t i = sc->depth;

    do
        i--;
    while (sc->stack[i].deffn && i);

    return sc->stack[i].name;
}

static const Binding *look(const Scop *const sc, const uint16_t name, uint16_t *const scope)
{
    uint32_t i = sc->depth;
    bool shared_only = false; /* only what the scope above sees */

    do {
        const ScopFrame *f = &sc->stack[--i];
        const Binding *b = scop_get(sc, f->name, name);

        if (b && b->flags != BF_USE && (!shared_only || (b->flags & BF_SHR))) {
            if ((b->flags & BF_REF) && i) { /* SHARED: the scope under it's, declared or not */
                f = &sc->stack[--i];
                b = scop_get(sc, f->name, name);

                if (!b)
                    b = &none;
            }

            *scope = f->name;
            return b;
        }

        shared_only = !f->deffn; /* under it, seen only when shared, unless a DEF FN's */
    } while (i);

    *scope = scop_own(sc);
    return &none;
}

const Binding *scop_look(const Scop *const sc, const uint16_t name, uint16_t *const scope)
{
    const uint32_t top = sc->stack[sc->depth - 1].name;

    if (!sc->look_on || sc->depth > 2) /* a DEF FN's: the stack more than its top */
        return look(sc, name, scope);

    const uint32_t key = (name | top << 16) ^ sc->depth << 31;
    /* kept in a const Scop: an answer, not a binding */
    ScopLook *const m = (ScopLook *)&sc->look[(name * 2654435761u ^ top * 40503u) >> 23 &
                                               (countof(sc->look) - 1)];

    if (m->key != key || m->gen != sc->gen || !m->b) {
        m->b = look(sc, name, &m->scope);
        m->key = key;
        m->gen = sc->gen;
    }

    *scope = m->scope;
    return m->b;
}

/* Name's binding as the procedure compiled sees it. */
static const Binding *scop_seen(const Scop *const sc, const uint16_t name)
{
    uint16_t scope;
    return scop_look(sc, name, &scope);
}

uint32_t scop_type(const Scop *const sc, const uint16_t name, const bool array)
{
    return scop_seen(sc, name)->et[array];
}

uint32_t scop_rec(const Scop *const sc, const uint16_t name, const bool array)
{
    return scop_seen(sc, name)->rec[array];
}

uint16_t scop_fix(const Scop *const sc, const uint16_t name, const bool array)
{
    return scop_seen(sc, name)->fix[array];
}

uint32_t scop_sfx(const Scop *const sc, const uint16_t name, const uint8_t type, const bool array)
{
    const Binding *const b = scop_seen(sc, name);

    if (b->flags & BF_CON)
        return b->et[0];

    const uint32_t et = b->et[array];

    if (b->rec[array] || (et && et != type))
        scop_fail(sc, E_RD);

    return type;
}

bool scop_con(const Scop *const sc, const uint16_t name)
{
    return scop_seen(sc, name)->flags & BF_CON;
}

bool scop_conname(const Scop *const sc, const uint16_t name)
{
    const Binding *const module = scop_get(sc, 0, name);
    const Binding *const own = scop_get(sc, scop_own(sc), name);
    return (module && module->flags & BF_CON) || (own && own->flags & BF_CON);
}

bool scop_known(const Scop *const sc, const uint16_t name) { return scop_seen(sc, name) != &none; }

bool scop_mshr(const Scop *const sc, const uint16_t name)
{
    const Binding *const b = scop_get(sc, 0, name);
    return b && (b->flags & (BF_SHR | BF_CON)) == BF_SHR;
}

/* Has a scope label key: a name, a line number when scope has SC_NUM? */
static bool scop_lany(
    const Scop *const sc,
    const Symb *const sy,
    const uint16_t scope,
    const uint16_t key
)
{
    if (scope & SC_NUM)
        return key / WORD_BITS < sc->nlnbit && sc->lnbit[key / WORD_BITS] >> (key % WORD_BITS) & 1;

    return symb_name(sy, key)->flags & NM_LBL;
}

void scop_label(
    Scop *const sc,
    Symb *const sy,
    const uint16_t scope,
    const uint16_t key,
    const uint32_t line
)
{
    if (scop_lany(sc, sy, scope, key))
        scop_fail(sc, E_DL);
    if (scope & SC_NUM) {
        if (!GROW(sc->lnbit, sc->nlnbit, key / WORD_BITS + 1u))
            scop_fail(sc, E_OM);

        sc->lnbit[key / WORD_BITS] |= 1u << (key % WORD_BITS);
    } else
        symb_name(sy, key)->flags |= NM_LBL;

    Binding *const b = scop_bind(sc, scope, key);
    b->flags |= BF_LBL;
    b->line = line;
}

uint32_t scop_lfind(
    const Scop *const sc,
    const Symb *const sy,
    const uint16_t scope,
    const uint16_t key
)
{
    const Binding *const b = scop_get(sc, scope, key);

    if (b && (b->flags & BF_LBL))
        return b->line;
    if (scop_lany(sc, sy, scope, key))
        scop_fail(sc, E_SPE);

    return 0;
}
