/* Op fusion: a statement's common sequences of ops run as one op each.
 *
 * # How it works
 * comp_flush hands fuse_code the code it just made, before the statement map records its end:
 * jumps go to statements' starts only, so a statement's code can shrink inside. Operands are
 * still the compiler's (variable ids, Jumps unresolved); link_resolve resolves a fused op's.
 *
 * The ops are copied one by one; after each, the rules are tried on the copy's last ops, again
 * after each rewrite, so a fused op can fuse further:
 *
 *   NEXT     I.LD.V c, I.STEP.V s, I.ST.V c, I.LD.V c, I.CMP.V lim m, I.JF j   I.NEXT.UP|DN c s lim j
 *   Quickr's I.LD.V c, I.LD.V lim, I.LD.V s, X.I.COUNT k, I.ST.V c, I.JT j      I.COUNT.UP|DN c s lim j
 *   MOD k    I.LD.U8 k, I.MOD                                                  I.MOD.U8 k
 *   op k     I.LD.I16 k, I.ADD .. I.MOD | I.CMP m                              I.ADD.I16 .. k [m]
 *   branch   I.CMP[.V|.U8|.I16] [x] m, I.JF|I.JT j                             I.JFC[...] [x] m j
 *   LONG's   [D.LD.V x,] L.CMP m, I.JF|I.JT j                                  L.JFC[.V] [x] m j
 *   x = a op b   I.LD.V a, I.ADD.V b .. I.MOD.I16 k, I.ST.V x                  I.ADD.V.ST x a b ..
 *   x = k, x = a I.LD.U8|I16|I32 k or I.LD.V a, I.ST.V x                       I.SET.U8 .. x k, I.MOV x a
 *   p.f      R.ADDR p, FL.LD off et, p a record parameter                      P.FL.LD p off et
 *
 * A JT is a JF on the other relations: its mask's complement. A procedure's local has a frame twin
 * of each one-cell op, and of NEXT when its three cells are all the frame's (comp/select.c); it is
 * in no other fused op of more cells, nor I.SET.
 *
 * The fused ops have no asm counterpart; tests/golden compares the code unfused.
 */
#include "comp/fuse.h"
#include "num/double.h"
#include "vm/exec.h"
#include "vm/flow.h"
#include "vm/hot.h"

enum { MAX_OPS = 512 }; /* a statement's ops fused; past them the rest are copied as they are */

typedef struct {
    const Symb *sy;
    uint8_t *at[MAX_OPS]; /* each copied op's start */
    uint32_t n;
} Tail;

/* Is the variable whose id is at p a global, not in a frame? */
static bool global(const Tail *const t, const uint8_t *const p)
{
    return !symb_in_frame(t->sy, load32(p));
}

/* The op k back from the last (0 the last). */
static uint8_t *back(const Tail *const t, const uint32_t k) { return t->at[t->n - 1 - k]; }

/* The last k ops replaced by the len bytes at op: the copy's new end. */
static uint8_t *replace(
    Tail *const t,
    const uint32_t k,
    const uint8_t *const op,
    const uint32_t len
)
{
    uint8_t *const start = back(t, k - 1);
    memmove(start, op, len);
    t->n -= k - 1;
    return start + len;
}

/* Are the last n ops those of ops, by their first bytes? */
static bool ends(const Tail *const t, const uint8_t *const ops, const uint32_t n)
{
    if (t->n < n)
        return false;

    for (uint32_t i = 0; i < n; i++)
        if (*back(t, n - 1 - i) != ops[i])
            return false;

    return true;
}

/* The last k ops as the fused NEXT op op: its counter, step and limit cells those at c, s and lim,
   its Jump j's. Null unless all three are global or all three in the frame: its frame twin
   (comp/select.c). */
static uint8_t *counted(
    Tail *const t,
    const uint32_t k,
    const uint8_t op,
    const uint8_t *const c,
    const uint8_t *const s,
    const uint8_t *const lim,
    const uint8_t *const j
)
{
    if (global(t, c) != global(t, s) || global(t, c) != global(t, lim))
        return nullptr;

    uint8_t to[NEXT_SIZE];
    to[0] = op;
    memcpy(to + 1, c, VCELL);
    memcpy(to + 1 + VCELL, s, VCELL);
    memcpy(to + 1 + 2 * VCELL, lim, VCELL);
    memcpy(to + 1 + 3 * VCELL, j + 1, sizeof(Jump));
    return replace(t, k, to, NEXT_SIZE);
}

/* NEXT's six ops, an INT's of a step whose sign is known (past by GT or LT). */
static uint8_t *next(Tail *const t)
{
    static const uint8_t ops[] = {O_I_LD_V, O_I_STEP_V, O_I_ST_V, O_I_LD_V, O_I_CMP_V, O_I_JF};

    if (!ends(t, ops, countof(ops)))
        return nullptr;

    const uint8_t *const ld = back(t, 5), *const step = back(t, 4), *const st = back(t, 3);
    const uint8_t *const ld2 = back(t, 2), *const cmp = back(t, 1);
    const uint32_t c = load32(ld + 1);
    const uint8_t m = cmp[1 + VCELL];

    if (load32(st + 1) != c || load32(ld2 + 1) != c || (m != DC_GT && m != DC_LT))
        return nullptr;

    const uint8_t op = m == DC_GT ? O_I_NEXT_UP : O_I_NEXT_DN;
    return counted(t, countof(ops), op, ld + 1, step + 1, cmp + 1, back(t, 0));
}

/* Quickr's NEXT, its six ops (qr/qcg_emit.c), of a step whose sign is known. */
static uint8_t *count(Tail *const t)
{
    static const uint8_t ops[] = {O_I_LD_V, O_I_LD_V, O_I_LD_V, O_X, O_I_ST_V, O_I_JT};

    if (!ends(t, ops, countof(ops)))
        return nullptr;

    const uint8_t *const ld = back(t, 5), *const lim = back(t, 4), *const step = back(t, 3);
    const uint8_t *const x = back(t, 2), *const st = back(t, 1);

    if (x[1] != X_I_COUNT || x[2] == COUNT_BY || load32(st + 1) != load32(ld + 1))
        return nullptr;

    const uint8_t op = x[2] == COUNT_UP ? O_I_COUNT_UP : O_I_COUNT_DN;
    return counted(t, countof(ops), op, ld + 1, step + 1, lim + 1, back(t, 0));
}

/* A literal's load and the INT operator after it: the operator with the literal. */
static uint8_t *literal(Tail *const t)
{
    if (t->n < 2)
        return nullptr;

    static const struct {
        uint8_t op, i16;
    } with[] = {
        {O_I_ADD, O_I_ADD_I16},
        {O_I_SUB, O_I_SUB_I16},
        {O_I_MUL, O_I_MUL_I16},
        {O_I_DIV, O_I_DIV_I16},
        {O_I_AND, O_I_AND_I16},
        {O_I_OR, O_I_OR_I16},
        {O_I_MOD, O_I_MOD_I16},
        {O_I_CMP, O_I_CMP_I16},
    };

    const uint8_t *const ld = back(t, 1), *const o = back(t, 0);
    uint8_t op[4];

    if (*ld == O_I_LD_U8 && *o == O_I_MOD) {
        op[0] = O_I_MOD_U8;
        op[1] = ld[1];
        return replace(t, 2, op, 2);
    }

    if (*ld != O_I_LD_I16)
        return nullptr;

    for (uint32_t i = 0; i < countof(with); i++)
        if (*o == with[i].op) {
            op[0] = with[i].i16;
            memcpy(op + 1, ld + 1, 2);

            if (*o != O_I_CMP)
                return replace(t, 2, op, 3);

            op[3] = o[1]; /* its mask */
            return replace(t, 2, op, 4);
        }

    return nullptr;
}

/* An INT compare and the JF or JT after it: one compare-and-branch. */
static uint8_t *branch(Tail *const t)
{
    if (t->n < 2)
        return nullptr;

    static const struct {
        uint8_t cmp, jfc, len; /* the compare's operand bytes */
    } with[] = {
        {O_I_CMP, O_I_JFC, 0},
        {O_I_CMP_V, O_I_JFC_V, VCELL},
        {O_I_CMP_U8, O_I_JFC_U8, 1},
        {O_I_CMP_I16, O_I_JFC_I16, 2},
    };

    const uint8_t *const cmp = back(t, 1), *const j = back(t, 0);

    if (*j != O_I_JF && *j != O_I_JT)
        return nullptr;

    for (uint32_t i = 0; i < countof(with); i++)
        if (*cmp == with[i].cmp) {
            const uint32_t n = with[i].len;
            uint8_t op[1 + VCELL + 1 + sizeof(Jump)];
            op[0] = with[i].jfc;
            memcpy(op + 1, cmp + 1, n);
            op[1 + n] = *j == O_I_JF ? cmp[1 + n] : ~cmp[1 + n] & (DC_GT | DC_EQ | DC_LT);
            memcpy(op + 2 + n, j + 1, sizeof(Jump));
            return replace(t, 2, op, 2 + n + sizeof(Jump));
        }

    return nullptr;
}

/* A LONG compare, its right operand a variable's load or not, and the JF or JT after it: one
   compare-and-branch. */
static uint8_t *lbranch(Tail *const t)
{
    if (t->n < 2)
        return nullptr;

    const uint8_t *const cmp = back(t, 1), *const j = back(t, 0);

    if (*cmp != O_L_CMP || (*j != O_I_JF && *j != O_I_JT))
        return nullptr;

    const bool var = t->n >= 3 && *back(t, 2) == O_D_LD_V;
    const uint32_t n = var ? VCELL : 0;
    uint8_t op[2 + VCELL + sizeof(Jump)];
    op[0] = var ? O_L_JFC_V : O_L_JFC;

    if (var)
        memcpy(op + 1, back(t, 2) + 1, VCELL);

    op[1 + n] = *j == O_I_JF ? cmp[1] : ~cmp[1] & (DC_GT | DC_EQ | DC_LT);
    memcpy(op + 2 + n, j + 1, sizeof(Jump));
    return replace(t, 2 + var, op, 2 + n + sizeof(Jump));
}

/* A store after an INT op on a load, or after a load alone: one op that stores. */
static uint8_t *store(Tail *const t)
{
    static const struct {
        uint8_t op, st;
    } with[] = {
#define X(NAME, name, src, op) {O_##NAME, O_##NAME##_ST},
        HOT_INT_STS(X)
#undef X
    };

    if (t->n < 2)
        return nullptr;

    const uint8_t *const st = back(t, 0), *const prev = back(t, 1);
    uint8_t op[1 + 3 * VCELL];

    if ((*st != O_I_ST_V && *st != O_F_ST_V) || !global(t, st + 1))
        return nullptr;

    memcpy(op + 1, st + 1, VCELL); /* the cell stored to first */

    if (*st == O_I_ST_V && t->n >= 3 && *back(t, 2) == O_I_LD_V)
        for (uint32_t i = 0; i < countof(with); i++)
            if (*prev == with[i].op) {
                if (!global(t, back(t, 2) + 1) || (exec_opflags(prev) & FV && !global(t, prev + 1)))
                    return nullptr;

                const uint32_t n = flow_opsize(prev) - 1; /* its operand */
                op[0] = with[i].st;
                memcpy(op + 1 + VCELL, back(t, 2) + 1, VCELL);
                memcpy(op + 1 + 2 * VCELL, prev + 1, n);
                return replace(t, 3, op, 1 + 2 * VCELL + n);
            }

    const bool ints = *st == O_I_ST_V;

    switch (*prev) { /* a load's bits stored as they are */
    case O_I_LD_U8:
    case O_I_LD_I16:
    case O_I_LD_I32:
        if (!ints)
            return nullptr;

        op[0] = *prev == O_I_LD_U8 ? O_I_SET_U8 : *prev == O_I_LD_I16 ? O_I_SET_I16 : O_I_SET_I32;
        break;

    case O_F_LD_F32:
        op[0] = O_I_SET_I32;
        break;

    case O_I_LD_V:
    case O_F_LD_V:
        if (!global(t, prev + 1))
            return nullptr;

        op[0] = O_I_MOV;
        break;

    default:
        return nullptr;
    }

    const uint32_t n = flow_opsize(prev) - 1;
    memcpy(op + 1 + VCELL, prev + 1, n);
    return replace(t, 2, op, 1 + VCELL + n);
}

/* A record parameter's address and the field's load after it: one load through its ref. */
static uint8_t *pfield(Tail *const t)
{
    if (t->n < 2)
        return nullptr;

    const uint8_t *const ra = back(t, 1), *const ld = back(t, 0);

    if (*ra != O_R_ADDR || *ld != O_FL_LD || !symb_is_ref(t->sy, load32(ra + 1)))
        return nullptr;

    uint8_t op[4 + VID];
    op[0] = O_P_FL_LD;
    memcpy(op + 1, ra + 1, VID);
    memcpy(op + 1 + VID, ld + 1, 3);
    return replace(t, 2, op, sizeof op);
}

uint8_t *fuse_code(const Symb *const sy, uint8_t *const start, uint8_t *const end)
{
    static Tail t;
    t.sy = sy;
    t.n = 0;
    uint8_t *out = start;

    for (const uint8_t *p = start; p < end;) {
        const uint32_t n = flow_opsize(p);

        if (out != p) /* none fused yet: in place */
            memmove(out, p, n);

        p += n;

        if (t.n == MAX_OPS) { /* the rest as it is */
            out += n;
            continue;
        }

        t.at[t.n++] = out;
        out += n;

        for (uint8_t *e;

             (e = next(&t)) || (e = count(&t)) || (e = literal(&t)) || (e = branch(&t)) ||
             (e = lbranch(&t)) || (e = store(&t)) || (e = pfield(&t));)
            out = e;
    }

    return out;
}
