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

static const char *expo_type_why(const Decl *d, uint32_t t);

/* Why field f has no type both dialects have; null it has. */
static const char *expo_field_why(const Decl *const d, const Field *const f)
{
    if (f->ndim || f->dyn)
        return "an array field";
    if (f->sub)
        return expo_type_why(d, f->sub) ? "a field of a TYPE Quickr can't use" : nullptr;
    if (f->et == ET_SD && f->fix)
        return "a fixed-length string field";
    if (f->et == ET_BY)
        return "a BYTE field";

    return nullptr;
}

/* Why TYPE t can't be used across; null it can. */
static const char *expo_type_why(const Decl *const d, const uint32_t t)
{
    const TypeDef *const td = decl_type(d, t);

    for (uint32_t k = 0; k < td->nfld; k++) {
        const char *const why = expo_field_why(d, &td->fields[k]);

        if (why)
            return why;
    }

    return nullptr;
}

/* Why a value has no type both dialects have: none, an array, a BYTE, a TYPE of one. */
typedef enum : uint8_t { EW_NONE, EW_ARRAY, EW_BYTE, EW_TYPE } ExpWhy;

static const char *const expo_param_why[] = {
    [EW_ARRAY] = "an array parameter",
    [EW_BYTE] = "a BYTE parameter",
    [EW_TYPE] = "a parameter of a TYPE Quickr can't use",
};

static const char *const expo_result_why[] = {
    [EW_ARRAY] = "an array result",
    [EW_BYTE] = "a BYTE result",
    [EW_TYPE] = "a result of a TYPE Quickr can't use",
};

/* Variable id's type as a value's to *et; why it has none both dialects have. */
static ExpWhy expo_var(const Comp *const C, const uint32_t id, uint32_t *const et)
{
    const Var *const v = symb_var(C->symb, id);

    if (symb_is_record(C->symb, id)) {
        *et = v->type;
        return expo_type_why(C->decl, v->type - ET_REC) ? EW_TYPE : EW_NONE;
    }

    if (v->info & VI_ARR)
        return EW_ARRAY;
    if (v->type == ET_BY)
        return EW_BYTE;

    const uint8_t k = v->info & VK_MASK;
    *et = v->type == ET_SH   ? ET_SH
          : k == VK_INT      ? ET_I4
          : k == VK_STR      ? ET_SD
          : v->info & SK_DBL ? ET_R8
                             : ET_R4;
    return EW_NONE;
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
    ExpProc e = {.proc = i, .npar = blok_nargs(p), .resarg = p->flags & PF_RESARG};

    e.name = expo_name(C, a, p->name, &e.len);
    e.params = expo_room(C, a, p->npar, sizeof *e.params);

    for (uint32_t k = 0; k < p->npar; k++) {
        const uint32_t id = p->params[k].var;
        const ExpWhy why = expo_var(C, id, &e.params[k].et);
        e.params[k].byref = !p->params[k].byval;
        e.params[k].name = expo_name(C, a, symb_var(C->symb, id)->name, &e.params[k].len);

        if (why && !e.why)
            e.why = k < e.npar ? expo_param_why[why] : expo_result_why[why];
    }

    const ExpWhy res = p->kind == PK_FUNC && !e.resarg ? expo_var(C, p->result, &e.result.et) : 0;

    if (res && !e.why)
        e.why = expo_result_why[res];
    if (e.resarg)
        e.result = e.params[e.npar];

    return e;
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
            *ef = (ExpField){.et = decl_fieldtype(f), .off = f->off};

            ef->name = expo_name(C, a, f->name, &ef->len);
        }
    }

    return x;
}

/* Name n declared again by an export: the clash *clash records, the first. */
static void expo_again(const Symb *const sy, const uint32_t n, uint32_t *const clash)
{
    const Name *const nm = symb_name(sy, n);

    if (!*clash && (nm->flags & (NM_SUB | NM_FUNC) || nm->rec))
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
        expo_again(sy, n, &clash);
        symb_name(sy, n)->flags |= fn ? NM_FUNC : NM_SUB;
        scop_bind(C->scop, sc_module(m), n)->flags |= BF_PROC;

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
        expo_again(sy, n, &clash);

        symb_name(sy, n)->rec = e->td;
        ((TypeDef *)btab_at(&C->decl->types, e->td))->alien = e->why;
    }

    return clash;
}
