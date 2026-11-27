/* A module's exports as the other dialect sees them.
 *
 * # How it works
 * Both front ends compile to the same VM: a procedure is a Proc called by PCALL, a record a TYPE,
 * a string a Desc, whichever dialect wrote it. An export is described in the VM's types; each
 * front end binds the other's exports as its own names (qr/qsem_mod.c, comp/declare.c), so a call
 * across is a call within one dialect. What one dialect has and the other not is kept with why,
 * an error where the name is used.
 */
#include "comp/export.h"
#include "comp/bind.h"
#include "comp/blocks.h"
#include "comp/comp.h"
#include "mem/scope.h"

/* A room of n entries of size from a; Out of memory. */
static void *expo_room(const Comp *const C, Arena *const a, const uint32_t n, const size_t size)
{
    void *const p = arena_alloc(a, (n ? n : 1) * size);

    if (!p)
        fault_error(C->fault, E_OM);

    return p;
}

/* The TYPEs being checked, the innermost first. */
typedef struct Within Within;
struct Within {
    uint32_t t;
    const Within *up;
};

static const char *expo_within_why(const Decl *d, uint32_t t, const Within *up);
static const char expo_holds_itself[] = "a TYPE that holds itself";

/* Why field f has no type both dialects have; null it has. An array of one dimension from 0, or a
   dynamic one, is a fixed array or a list of its elements. */
static const char *expo_field_why(const Decl *const d, const Field *const f, const Within *const up)
{
    if (f->ndim > 1 || (f->ndim && decl_fielddim(d, f, 0)->lb))
        return "an array field of more than one dimension, or not from 0";
    if (f->sub) {
        const char *const why = expo_within_why(d, f->sub, up);
        return why == expo_holds_itself ? why
               : why                    ? "a field of a TYPE Quickr can't use"
                                        : nullptr;
    }

    if (f->et == ET_SD && f->fix)
        return "a fixed-length string field";
    if (f->et == ET_BY)
        return "a BYTE field";

    return nullptr;
}

/* Why TYPE t, inside up's, can't be used across; null it can. One holding itself, through a
   dynamic array, can't. */
static const char *expo_within_why(const Decl *const d, const uint32_t t, const Within *const up)
{
    for (const Within *w = up; w; w = w->up)
        if (w->t == t)
            return expo_holds_itself;

    const TypeDef *const td = decl_type(d, t);
    const Within in = {.t = t, .up = up};

    for (uint32_t k = 0; k < td->nfld; k++) {
        const char *const why = expo_field_why(d, &td->fields[k], &in);

        if (why)
            return why;
    }

    return nullptr;
}

/* Why TYPE t can't be used across; null it can. */
static const char *expo_type_why(const Decl *const d, const uint32_t t)
{
    return expo_within_why(d, t, nullptr);
}

static const char *const expo_param_why[] = {
    [EW_BYTE] = "a BYTE parameter",
    [EW_TYPE] = "a parameter of a TYPE Quickr can't use",
    [EW_FIXED] = "a fixed-length string parameter",
};

static const char *const expo_result_why[] = {
    [EW_ARRAY] = "an array result",
    [EW_BYTE] = "a BYTE result",
    [EW_TYPE] = "a result of a TYPE Quickr can't use",
};

ExpWhy expo_value_why(const Decl *const d, const Binding *const b, const bool arr)
{
    if (b->rec[arr])
        return expo_type_why(d, b->rec[arr]) ? EW_TYPE : EW_NONE;
    if (b->fix[arr])
        return EW_FIXED;

    return b->et[arr] == ET_BY && !arr ? EW_BYTE : EW_NONE; /* a BYTE array: a ubyte list */
}

/* Variable id's type as a value's, an array's its elements'; a BYTE scalar's verdict declared. */
static uint32_t expo_var(const Comp *const C, const uint32_t id)
{
    const Var *const v = symb_var(C->symb, id);

    if (symb_is_record(C->symb, id))
        return v->type;

    const uint8_t k = v->info & VK_MASK;
    return v->type == ET_SH || v->type == ET_BY ? v->type
           : k == VK_INT                        ? ET_I4
           : k == VK_STR                        ? ET_SD
           : v->info & SK_DBL                   ? ET_R8
                                                : ET_R4;
}

/* Name n's text as first written, copied to a. */
static const char *expo_name(const Comp *const C, Arena *const a, const uint32_t n, uint32_t *len)
{
    const uint8_t *const t = symb_text(C->symb, n);
    char *const s = expo_room(C, a, t[0] + 1u, 1);
    memcpy(s, t + 1 + t[0], t[0]);
    *len = t[0];
    return s;
}

/* Procedure p, QBasic's, as an export. */
static ExpProc expo_proc(const Comp *const C, Arena *const a, const Proc *const p, const uint32_t i)
{
    ExpProc e = {
        .proc = i,
        .npar = blok_nargs(p),
        .resarg = p->flags & PF_RESARG,
        .native = p->flags & PF_NATIVE,
    };

    e.name = expo_name(C, a, p->name, &e.len);
    e.params = expo_room(C, a, p->npar, sizeof *e.params);

    for (uint32_t k = 0; k < p->npar; k++) {
        const uint32_t id = p->params[k].var;
        e.params[k].et = expo_var(C, id);
        e.params[k].byref = !p->params[k].byval;
        e.params[k].array = symb_is_array(C->symb, id);
        e.params[k].name = expo_name(C, a, symb_var(C->symb, id)->name, &e.params[k].len);
    }

    if (p->kind == PK_FUNC && !e.resarg)
        e.result.et = expo_var(C, p->result);

    const Binding *const b = scop_get(C->scop, p->module, p->name);
    const uint32_t why = b ? (uint32_t)b->val : 0;
    e.why = why % EXP_RESULT ? expo_param_why[why % EXP_RESULT]
            : why            ? expo_result_why[why / EXP_RESULT]
                             : nullptr;

    if (e.resarg)
        e.result = e.params[e.npar];

    return e;
}

/* Is name t QBasic module m's CONST, exported? */
static bool expo_is_const(const Comp *const C, const uint32_t m, const uint32_t t)
{
    return symb_name(C->symb, t)->module == m && bind_exports(C->scop, C->symb, t) & BIND_CONST;
}

/* Module m's variable of name t, a scalar of type et (comp_var's). */
static uint32_t expo_scalar(
    const Comp *const C,
    const uint32_t m,
    const uint32_t t,
    const uint32_t et
)
{
    return symb_find_var(C->symb, t, et, comp_etkind[et], sc_module(m), false);
}

/* QBasic module m's CONSTs into x: an integer's value known as declared, any other's variable. */
static void expo_consts(const Comp *const C, const uint32_t m, Arena *const a, Exports *const x)
{
    uint32_t n = 0;

    for (uint32_t t = 1; t < C->symb->nnames; t++)
        n += expo_is_const(C, m, t);

    x->consts = expo_room(C, a, n, sizeof *x->consts);

    for (uint32_t t = 1; t < C->symb->nnames; t++) {
        if (!expo_is_const(C, m, t))
            continue;

        const Binding *const b = scop_get(C->scop, sc_module(m), t);
        ExpConst *const e = &x->consts[x->nconsts++];
        *e = (ExpConst){.et = b->et[0], .known = b->flags & BF_VAL, .i = b->val};

        e->name = expo_name(C, a, t, &e->len);

        if (!e->known)
            e->var = expo_scalar(C, m, t, e->et);
    }
}

Exports expo_qbasic(const Comp *const C, const uint32_t m, Arena *const a)
{
    const Blok *const b = C->blok;
    Exports x = {
        .procs = expo_room(C, a, b->procs.n, sizeof *x.procs),
        .types = expo_room(C, a, C->decl->ntypes + 1, sizeof *x.types),
    };

    for (uint32_t i = 1; i < b->procs.n; i++) {
        const Proc *const p = blok_proc(b, i);

        if (p->module == sc_module(m) && !(p->flags & (PF_DEF | PF_FOREIGN)) &&
            bind_exports(C->scop, C->symb, p->name) & BIND_PROC)
            x.procs[x.nprocs++] = expo_proc(C, a, p, i);
    }

    for (uint32_t t = 1; t <= C->decl->ntypes; t++) {
        const TypeDef *const td = decl_type(C->decl, t);
        const Name *const n = td->name ? symb_name(C->symb, td->name) : nullptr;

        if (!n || n->module != m || !(bind_exports(C->scop, C->symb, td->name) & BIND_TYPE))
            continue;

        ExpType *const e = &x.types[x.ntypes++];
        *e = (ExpType){
            .td = t,
            .fields = expo_room(C, a, td->nfld, sizeof *e->fields),
            .nfields = td->nfld,
            .size = td->size,
            .deep = decl_dynamic(td),
            .why = expo_type_why(C->decl, t),
        };

        e->name = expo_name(C, a, td->name, &e->len);

        for (uint32_t k = 0; k < td->nfld; k++) {
            const Field *const f = &td->fields[k];
            ExpField *const ef = &e->fields[k];
            *ef = (ExpField){
                .et = decl_fieldtype(f),
                .off = f->off,
                .count = f->ndim ? decl_fielddim(C->decl, f, 0)->count : 0,
                .list = f->dyn,
            };

            ef->name = expo_name(C, a, f->name, &ef->len);
        }
    }

    expo_consts(C, m, a, &x);
    return x;
}

bool expo_usable(const Comp *const C, const uint32_t t)
{
    const Name *const n = symb_name(C->symb, t);
    const Binding *const b = scop_get(C->scop, sc_module(n->module), t);
    const TypeDef *const td = n->rec ? decl_type(C->decl, n->rec) : nullptr;

    return !(b && b->flags & BF_PROC && (b->flags & BF_ALIEN || b->val)) &&
           !(td && (td->alien || expo_type_why(C->decl, n->rec)));
}

/* Name n of module m declared again by an export: the clash *clash records, the first. */
static void expo_again(
    const Comp *const C,
    const uint32_t m,
    const uint32_t n,
    uint32_t *const clash
)
{
    const Name *const nm = symb_name(C->symb, n);
    const Binding *const b = scop_get(C->scop, sc_module(m), n);

    if (!*clash && (nm->flags & (NM_SUB | NM_FUNC) || nm->rec || (b && b->flags & BF_CON)))
        *clash = n;
}

uint32_t expo_declare(Comp *const C, const uint32_t m, const Exports *const x)
{
    Symb *const sy = C->symb;
    uint32_t clash = 0;

    for (uint32_t i = 0; i < x->nprocs; i++) {
        const ExpProc *const e = &x->procs[i];
        const uint32_t n = symb_find_name(sy, m, (const uint8_t *)e->name, (uint8_t)e->len);
        const bool fn = e->result.et != ET_IMP;
        expo_again(C, m, n, &clash);
        symb_name(sy, n)->flags |= fn ? NM_FUNC : NM_SUB;
        scop_bind(C->scop, sc_module(m), n)->flags |= BF_PROC | (e->why ? BF_ALIEN : 0);

        if (!fn)
            continue;

        Binding *const b = scop_bind(C->scop, n, n); /* its result's type, as a FUNCTION's AS */
        b->flags |= BF_RES;

        if (e->result.et >= ET_REC) {
            b->rec[0] = e->result.et - ET_REC;
            symb_name(sy, n)->flags |= NM_REC;
        } else
            b->et[0] = (uint8_t)e->result.et;
    }

    for (uint32_t i = 0; i < x->ntypes; i++) {
        const ExpType *const e = &x->types[i];
        const uint32_t n = symb_find_name(sy, m, (const uint8_t *)e->name, (uint8_t)e->len);
        expo_again(C, m, n, &clash);

        symb_name(sy, n)->rec = e->td;
        ((TypeDef *)btab_at(&C->decl->types, e->td))->alien = e->why;
    }

    for (uint32_t i = 0; i < x->nconsts; i++) {
        const ExpConst *const e = &x->consts[i];
        const uint32_t n = symb_find_name(sy, m, (const uint8_t *)e->name, (uint8_t)e->len);
        expo_again(C, m, n, &clash);

        Binding *const b = scop_bind(C->scop, sc_module(m), n);
        const bool whole = e->et != ET_R4 && e->et != ET_R8 && e->et != ET_SD;
        b->flags |= BF_CON | BF_SHR | (whole ? BF_VAL : 0); /* decl_int's: a TYPE's, a DIM's */
        b->et[0] = (uint8_t)e->et;
        b->val = whole ? (int32_t)e->i : 0;
    }

    return clash;
}

void expo_literals(Comp *const C, const uint32_t m, const Exports *const x, Arena *const a)
{
    for (uint32_t i = 0; i < x->nconsts; i++) {
        const ExpConst *const e = &x->consts[i];
        const uint32_t n = symb_find_name(C->symb, m, (const uint8_t *)e->name, (uint8_t)e->len);
        const uint32_t id = expo_scalar(C, m, n, e->et);
        uint8_t *const k = symb_kval(C->symb, id);
        uint8_t op = O_I_LD_I32;
        uint64_t raw = (uint32_t)e->i;

        if (e->et == ET_R4)
            op = O_F_LD_F32, raw = (Cell){.f = (float)e->d}.u;
        else if (e->et == ET_R8)
            op = O_D_LD_K, raw = (Cell){.d = e->d}.raw;

        if (e->et == ET_SD) { /* S.LD.TXT len, its text's Addr: a room that stays */
            char *const text = expo_room(C, a, e->slen, 1);
            memcpy(text, e->s, e->slen);
            op = O_S_LD_TXT;
            raw = (uint8_t)e->slen | (uint64_t)addr_of(text) << 8;
        }

        for (uint32_t b = 0; b < 8; b++)
            k[b] = (uint8_t)(raw >> 8 * b);

        symb_varx(C->symb, id)->kop = op;
    }
}
