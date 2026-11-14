#include "comp/ref.h"
#include "session.h"
#include "comp/decl.h"
#include "comp/dfa.h"
#include "io/msg.h"
#include "io/repl.h"
#include "mem/scop.h"
#include "mem/symb.h"

[[noreturn]] static void synerr(Session *const S) { repl_error(S, E_SN); }
[[noreturn]] static void mismatch(Session *const S) { repl_error(S, E_TM); }

static Node *node(Session *const S, const uint16_t i) { return ast_node(S, i); }

/* ---- records */

/* rc_nrec, rc_nreca: the TYPE of record name, of its array; 0 none. A parameter of the procedure
   compiled is its own, else as DIM declared the name. */
static uint8_t nrec(Session *const S, const uint16_t name, const bool array)
{
    uint16_t id;
    if (comp_param(S, name, VI_ARR, &id)) { /* a record parameter, either kind */
        const uint8_t t = symb_var(&S->symb, id)->type;
        return t >= ET_REC ? t - ET_REC : 0;
    }
    if (!array && comp_param(S, name, 0, &id))
        return 0; /* a parameter, not a record */
    return scop_rec(S, name, array);
}

static uint16_t recvar(Session *const S, const uint16_t name, const uint8_t t, const uint8_t kind)
{
    comp_scope(S, name);
    return symb_find_var(S, name, ET_REC + t, kind, S->comp.scope, S->comp.isstatic);
}

uint16_t ref_recarray(Session *const S, const uint16_t name, const uint8_t t) { return recvar(S, name, t, VI_ARR); }

/* rc_whole: all of a record of TYPE t, at off. */
static RecRef whole(Session *const S, const uint8_t t, const uint16_t off)
{
    return (RecRef){.et = ET_REC + t, .off = off, .fix = S->decl.types[t].size};
}

/* rc_path: the fields f.g.. of TYPE t, the text len long. */
static RecRef path(Session *const S, uint8_t t, const uint8_t *text, uint32_t len)
{
    uint16_t off = 0;
    if (text[len - 1] < '.') /* a suffix is not the name's */
        len--;
    const Field *f;
    for (;;) {
        uint32_t n = 0;
        while (n < len && text[n] != '.')
            n++;
        const uint16_t name = symb_find_name(S, text, (uint8_t)n);
        const TypeDef *const td = &S->decl.types[t];
        f = nullptr;
        for (uint32_t i = 0; i < td->nfld && !f; i++)
            if (S->decl.fields[td->first + i].name == name)
                f = &S->decl.fields[td->first + i];
        if (!f)
            synerr(S);
        off += f->off;
        text += n;
        len -= n;
        if (!len)
            break;
        if (!f->sub) /* .g: a field of a TYPE's */
            synerr(S);
        t = f->sub;
        text++;
        len--;
    }
    if (f->sub) /* a record in it */
        return whole(S, f->sub, off);
    return (RecRef){.et = f->et, .off = off, .fix = f->fix};
}

/* An N_REF of op and variable id; an element's subscripts follow at comp.p. */
static uint16_t place(Session *const S, const uint8_t op, const uint16_t id)
{
    const uint16_t n = ast_new(S, N_REF);
    node(S, n)->op = op;
    node(S, n)->id = id;
    node(S, n)->b = NO_NODE;
    node(S, n)->type = T_INT;
    if (op == O_RA_ADDR)
        expr_args(S, n);
    return n;
}

uint16_t ref_record(Session *const S)
{
    const uint8_t *const p = S->comp.p;
    const uint8_t len = p[1];
    const uint16_t name = comp_nameof(p);
    const uint8_t *const after = comp_skipname(p);
    uint8_t t;
    if (*after == '(') { /* a(i) */
        t = nrec(S, name, true);
        if (!t)
            return NO_NODE;
        S->comp.p = after;
        const uint16_t n = place(S, O_RA_ADDR, ref_recarray(S, name, t));
        if (S->comp.p[0] == '.' && S->comp.p[1] == M_VAR) { /* .f */
            S->comp.p++;
            node(S, n)->rec = path(S, t, S->comp.p + 2, S->comp.p[1]);
            S->comp.p = decl_item(S->comp.p);
        } else
            node(S, n)->rec = whole(S, t, 0);
        return n;
    }
    t = nrec(S, name, false);
    if (t) { /* r */
        S->comp.p = after;
        const uint16_t n = place(S, O_R_ADDR, recvar(S, name, t, VI_RECARR));
        node(S, n)->rec = whole(S, t, 0);
        return n;
    }
    const uint8_t *const text = p + 2; /* r.f: the longest prefix a record's */
    for (uint32_t i = len - 1; (int32_t)i > 0; i--) {
        if (text[i] != '.')
            continue;
        const uint16_t pre = symb_find_name(S, text, (uint8_t)i);
        t = nrec(S, pre, false);
        if (!t)
            continue;
        const uint16_t n = place(S, O_R_ADDR, recvar(S, pre, t, VI_RECARR));
        node(S, n)->rec = path(S, t, text + i + 1, len - i - 1);
        S->comp.p = after;
        return n;
    }
    return NO_NODE;
}

uint16_t ref_field(Session *const S, const uint16_t ref)
{
    const uint8_t et = node(S, ref)->rec.et;
    if (et >= ET_REC)
        mismatch(S);
    const uint16_t n = ast_new(S, N_FIELD);
    node(S, n)->a = ref;
    node(S, n)->type = et == ET_SD ? T_STR : expr_ktype[comp_etkind[et]];
    node(S, n)->sh = et == ET_SH ? SH_SHORT : SH_NONE;
    return n;
}

/* ---- places */

/* Variable id's place: its kind and bytes; a string's Desc and its STRING * n. */
static uint32_t vakind(Session *const S, const uint16_t id)
{
    const Var *const v = symb_var(&S->symb, id);
    if (v->info & SK_DBL)
        return va_pack(VA_RAW, 8);
    switch (v->info & VK_MASK) {
    case VK_INT: /* 2 bytes but a LONG's */
        return va_pack(v->type == ET_I4 ? VA_RAW : VA_I2, 4);
    case VK_STR:
        return va_pack(VA_DESC, scop_fix(S, v->name, v->info & VI_ARR));
    default:
        return va_pack(VA_RAW, 4);
    }
}

/* A record's place: its bytes. */
static uint32_t field_size(const RecRef *const r)
{
    return r->et >= ET_REC || r->et == ET_SD ? r->fix : decl_etsize[r->et];
}

uint16_t ref_place(Session *const S, uint32_t *const vk)
{
    if (comp_cur(&S->comp) != M_VAR)
        synerr(S);
    const uint16_t r = ref_record(S);
    if (r != NO_NODE) {
        const RecRef *const rec = &node(S, r)->rec;
        *vk = va_pack(rec->et == ET_I2 ? VA_I2 : VA_RAW, field_size(rec));
        return r;
    }
    const uint16_t id = comp_var(S, 0, 0);
    *vk = vakind(S, id);
    if (symb_var(&S->symb, id)->info & VI_ARR) /* an element: RA.ADDR's */
        return place(S, O_RA_ADDR, id);
    if (comp_ispar(S, id)) /* a parameter's: its cell, the argument's */
        return place(S, O_V_ADDR, id);
    return expr_intleaf(S, symb_cell(&S->symb, id), SH_NONE);
}

bool ref_len(Session *const S, uint32_t *const size)
{
    const uint8_t *const p = S->comp.p;
    const uint16_t nodes = S->ast.nnodes;
    S->comp.p++;
    bool found = false;
    if (comp_cur(&S->comp) == '(' && comp_curn(&S->comp) == M_VAR) {
        const uint16_t r = ref_record(S);
        if (r != NO_NODE) {
            *size = field_size(&node(S, r)->rec);
            found = comp_cur(&S->comp) == ')';
        } else { /* a scalar: % 2, & ! 4, # 8 */
            const uint16_t id = comp_var(S, 0, 0);
            if (!(symb_var(&S->symb, id)->info & VI_ARR)) {
                const uint32_t v = vakind(S, id);
                *size = va_kind(v) == VA_I2 ? 2 : (uint8_t)v;
                found = comp_cur(&S->comp) == ')' && va_kind(v) != VA_DESC;
            }
        }
    }
    S->ast.nnodes = nodes; /* a size: the place's nodes not needed */
    if (found)
        S->comp.p++;
    else
        S->comp.p = p;
    return found;
}

/* ---- procedures */

const Proc *ref_proc(Session *const S, uint8_t *const idx)
{
    const uint8_t *const p = S->comp.p;
    if (*p != M_VAR)
        return nullptr;
    const Name *const n = symb_name(&S->symb, comp_nameof(p));
    if (!(n->flags & (NM_SUB | NM_FUNC | NM_FN)) || !n->proc)
        return nullptr;
    S->comp.p = comp_skipname(p);
    *idx = n->proc;
    return &S->blok.procs[n->proc];
}

void ref_dropargs(Session *const S, const bool paren)
{
    const uint16_t nodes = S->ast.nnodes;
    for (;;) {
        if (paren ? *S->comp.p == ')' : comp_end(S))
            break;
        if (*S->comp.p == TK_BYVAL)
            S->comp.p++;
        const uint8_t *const p = S->comp.p;
        const uint8_t *const after = *p == M_VAR ? comp_skipname(p) : p;
        if (*p == M_VAR && after[0] == '(' && after[1] == ')')
            S->comp.p = after + 2; /* a() an array whole */
        else
            expr_arg(S);
        if (*S->comp.p != ',')
            break;
        S->comp.p++;
    }
    S->ast.nnodes = nodes;
}

/* ct_aend: c ends an argument. */
static bool aend(const uint8_t c) { return !c || c == ',' || c == ')' || c == ':' || c == TK_REM; }

/* ct_ptm: variable id parameter par's type; else Parameter type mismatch. */
static void ptm(Session *const S, const uint16_t id, const uint16_t par)
{
    if (symb_var(&S->symb, id)->type != symb_var(&S->symb, par)->type)
        repl_error(S, E_PTM);
}

static uint16_t argnode(Session *const S, const uint8_t pass, const uint16_t a, const uint16_t id)
{
    const uint16_t n = ast_new(S, N_ARG);
    node(S, n)->op = pass;
    node(S, n)->a = a;
    node(S, n)->id = id;
    return n;
}

/* An array parameter's argument: n() an array whole; a record parameter's n a record whole. */
static uint16_t arrayarg(Session *const S, const uint16_t par)
{
    if (comp_cur(&S->comp) != M_VAR)
        synerr(S);
    const uint16_t name = comp_nameof(S->comp.p);
    const uint8_t *const after = comp_skipname(S->comp.p);
    if (*after == '(') {
        const uint8_t t = scop_rec(S, name, true);
        uint16_t id;
        if (t) {
            S->comp.p = after;
            id = ref_recarray(S, name, t);
        } else
            id = comp_var(S, 0, 0);
        if (!(symb_var(&S->symb, id)->info & VI_ARR))
            synerr(S);
        ptm(S, id, par);
        comp_open(S);
        comp_close(S);
        return argnode(S, PA_ARRAY, NO_NODE, id);
    }
    const uint8_t t = scop_rec(S, name, false);
    if (!t)
        synerr(S);
    S->comp.p = after;
    return argnode(S, PA_RECORD, NO_NODE, recvar(S, name, t, VI_RECARR));
}

/* ct_arg: an argument to parameter par: a variable alone by reference, an element or a record's
   number field by P.REF, else the value. A CONST, BYVAL's, a DEF FN's: the value. */
static uint16_t arg(Session *const S, const uint16_t par, const bool byval)
{
    if (symb_var(&S->symb, par)->info & VI_ARR)
        return arrayarg(S, par);
    if (!byval && comp_cur(&S->comp) == M_VAR) {
        const uint16_t name = comp_nameof(S->comp.p);
        if (!(symb_name(&S->symb, name)->flags & (NM_SUB | NM_FUNC | NM_FN)) && !scop_con(S, name)) {
            const uint8_t *const p = S->comp.p;
            const uint8_t *const after = comp_skipname(p);
            const uint16_t nodes = S->ast.nnodes;
            const uint8_t ptype = symb_var(&S->symb, par)->type;
            const uint16_t r = ref_record(S);
            if (r != NO_NODE) { /* a field */
                const RecRef *const rec = &node(S, r)->rec;
                if (!(rec->et == ET_SD && rec->fix) && rec->et != ET_SH && rec->et < ET_REC &&
                    rec->et == ptype && aend(comp_cur(&S->comp)))
                    return argnode(S, PA_PREF, r, par);
            } else if (*after == '(') { /* an element */
                const uint16_t id = comp_var(S, 0, 0);
                const Var *const v = symb_var(&S->symb, id);
                if (v->info & VI_ARR && !scop_fix(S, v->name, true) && v->type == ptype) {
                    const uint16_t e = place(S, O_RA_ADDR, id);
                    if (aend(comp_cur(&S->comp)))
                        return argnode(S, PA_PREF, e, par);
                }
            }
            S->comp.p = p;
            S->ast.nnodes = nodes;
            if (aend(*after)) { /* a variable alone: its cell */
                const uint16_t id = comp_var(S, 0, 0);
                ptm(S, id, par);
                return argnode(S, PA_VAR, place(S, O_V_ADDR, id), par);
            }
        }
    }
    return argnode(S, PA_VALUE, expr_arg(S), par);
}

/* ct_args: for each of the procedure's parameters, its argument, to call n. */
static void args(Session *const S, const uint16_t n, const Proc *const pr)
{
    const bool def = pr->flags & PF_DEF; /* a DEF FN's: by value */
    uint16_t last = NO_NODE;
    for (uint32_t i = 0; i < pr->npar; i++) {
        if (i) {
            if (comp_cur(&S->comp) != ',')
                synerr(S);
            S->comp.p++;
        }
        const bool byval = S->blok.byval[pr->par + i] || def;
        ast_link(S, n, &last, arg(S, S->blok.params[pr->par + i], byval));
    }
}

uint16_t ref_call(Session *const S, const uint8_t idx, const Proc *const pr, const ArgsForm form)
{
    const uint16_t n = ast_new(S, N_CALL);
    node(S, n)->op = idx;
    node(S, n)->b = NO_NODE;
    if (pr->result)
        node(S, n)->type = expr_ktype[symb_var(&S->symb, pr->result)->info & (SK_DBL | VK_MASK)];
    if (form == ARGS_BARE)
        args(S, n, pr);
    else if (form == ARGS_PAREN && *S->comp.p == '(') {
        S->comp.p++;
        args(S, n, pr);
        comp_close(S);
    }
    return n;
}

uint16_t ref_undef(Session *const S)
{
    const uint16_t id = comp_var(S, 1, 0);
    if (*S->comp.p == '(') {
        S->comp.p++;
        ref_dropargs(S, true);
        comp_close(S);
    }
    const uint16_t n = ast_new(S, N_CALL);
    node(S, n)->mode = CALL_UNDEF;
    node(S, n)->id = id;
    node(S, n)->b = NO_NODE;
    node(S, n)->type = expr_ktype[symb_var(&S->symb, id)->info & (SK_DBL | VK_MASK)];
    return n;
}
