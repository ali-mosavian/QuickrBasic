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
static Node *place(Session *const S, const uint8_t op, const uint16_t id)
{
    Node *const n = ast_new(S, N_REF);
    n->op = op;
    n->id = id;
    n->b = nullptr;
    n->type = T_INT;
    if (op == O_RA_ADDR)
        expr_args(S, n);
    return n;
}

Node *ref_record(Session *const S)
{
    const uint8_t *const p = S->comp.p;
    const uint8_t len = p[1];
    const uint16_t name = comp_nameof(p);
    const uint8_t *const after = comp_skipname(p);
    uint8_t t;
    if (*after == '(') { /* a(i) */
        t = nrec(S, name, true);
        if (!t)
            return nullptr;
        S->comp.p = after;
        Node *const n = place(S, O_RA_ADDR, ref_recarray(S, name, t));
        if (S->comp.p[0] == '.' && S->comp.p[1] == M_VAR) { /* .f */
            S->comp.p++;
            n->rec = path(S, t, S->comp.p + 2, S->comp.p[1]);
            S->comp.p = decl_item(S->comp.p);
        } else
            n->rec = whole(S, t, 0);
        return n;
    }
    t = nrec(S, name, false);
    if (t) { /* r */
        S->comp.p = after;
        Node *const n = place(S, O_R_ADDR, recvar(S, name, t, VI_RECARR));
        n->rec = whole(S, t, 0);
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
        Node *const n = place(S, O_R_ADDR, recvar(S, pre, t, VI_RECARR));
        n->rec = path(S, t, text + i + 1, len - i - 1);
        S->comp.p = after;
        return n;
    }
    return nullptr;
}

Node *ref_field(Session *const S, Node *const ref)
{
    const uint8_t et = ref->rec.et;
    if (et >= ET_REC)
        mismatch(S);
    Node *const n = ast_new(S, N_FIELD);
    n->a = ref;
    n->type = et == ET_SD ? T_STR : expr_ktype[comp_etkind[et]];
    n->sh = et == ET_SH ? SH_SHORT : SH_NONE;
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

Node *ref_place(Session *const S, uint32_t *const vk)
{
    if (comp_cur(&S->comp) != M_VAR)
        synerr(S);
    Node *const r = ref_record(S);
    if (r != nullptr) {
        const RecRef *const rec = &r->rec;
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
    S->comp.p++;
    bool found = false;
    if (comp_cur(&S->comp) == '(' && comp_curn(&S->comp) == M_VAR) {
        Node *const r = ref_record(S);
        if (r != nullptr) {
            *size = field_size(&r->rec);
            found = comp_cur(&S->comp) == ')';
        } else { /* a scalar: % 2, & ! 4, # 8 */
            const uint16_t id = comp_var(S, 0, 0);
            if (!(symb_var(&S->symb, id)->info & VI_ARR)) {
                const uint32_t v = vakind(S, id);
                *size = va_kind(v) == VA_I2 ? 2 : (uint8_t)v;
                found = comp_cur(&S->comp) == ')' && va_kind(v) != VA_DESC;
            }
        }
    } /* a size: the place's nodes not needed */
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
    return blok_proc(&S->blok, n->proc);
}

void ref_dropargs(Session *const S, const bool paren)
{
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
}

/* ct_aend: c ends an argument. */
static bool aend(const uint8_t c) { return !c || c == ',' || c == ')' || c == ':' || c == TK_REM; }

/* ct_ptm: variable id parameter par's type; else Parameter type mismatch. */
static void ptm(Session *const S, const uint16_t id, const uint16_t par)
{
    if (symb_var(&S->symb, id)->type != symb_var(&S->symb, par)->type)
        repl_error(S, E_PTM);
}

static Node *argnode(Session *const S, const uint8_t pass, Node *const a, const uint16_t id)
{
    Node *const n = ast_new(S, N_ARG);
    n->op = pass;
    n->a = a;
    n->id = id;
    return n;
}

/* An array parameter's argument: n() an array whole; a record parameter's n a record whole. */
static Node *arrayarg(Session *const S, const uint16_t par)
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
        return argnode(S, PA_ARRAY, nullptr, id);
    }
    const uint8_t t = scop_rec(S, name, false);
    if (!t)
        synerr(S);
    S->comp.p = after;
    return argnode(S, PA_RECORD, nullptr, recvar(S, name, t, VI_RECARR));
}

/* ct_arg: an argument to parameter par: a variable alone by reference, an element or a record's
   number field by P.REF, else the value. A CONST, BYVAL's, a DEF FN's: the value. */
static Node *arg(Session *const S, const uint16_t par, const bool byval)
{
    if (symb_var(&S->symb, par)->info & VI_ARR)
        return arrayarg(S, par);
    if (!byval && comp_cur(&S->comp) == M_VAR) {
        const uint16_t name = comp_nameof(S->comp.p);
        if (!(symb_name(&S->symb, name)->flags & (NM_SUB | NM_FUNC | NM_FN)) && !scop_con(S, name)) {
            const uint8_t *const p = S->comp.p;
            const uint8_t *const after = comp_skipname(p);
            const uint8_t ptype = symb_var(&S->symb, par)->type;
            Node *const r = ref_record(S);
            if (r != nullptr) { /* a field */
                const RecRef *const rec = &r->rec;
                if (!(rec->et == ET_SD && rec->fix) && rec->et != ET_SH && rec->et < ET_REC &&
                    rec->et == ptype && aend(comp_cur(&S->comp)))
                    return argnode(S, PA_PREF, r, par);
            } else if (*after == '(') { /* an element */
                const uint16_t id = comp_var(S, 0, 0);
                const Var *const v = symb_var(&S->symb, id);
                if (v->info & VI_ARR && !scop_fix(S, v->name, true) && v->type == ptype) {
                    Node *const e = place(S, O_RA_ADDR, id);
                    if (aend(comp_cur(&S->comp)))
                        return argnode(S, PA_PREF, e, par);
                }
            }
            S->comp.p = p;
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
static void args(Session *const S, Node *const n, const Proc *const pr)
{
    const bool def = pr->flags & PF_DEF; /* a DEF FN's: by value */
    Node *last = nullptr;
    for (uint32_t i = 0; i < pr->npar; i++) {
        if (i) {
            if (comp_cur(&S->comp) != ',')
                synerr(S);
            S->comp.p++;
        }
        const bool byval = pr->params[i].byval || def;
        ast_link(n, &last, arg(S, pr->params[i].var, byval));
    }
}

Node *ref_call(Session *const S, const uint8_t idx, const Proc *const pr, const ArgsForm form)
{
    Node *const n = ast_new(S, N_CALL);
    n->op = idx;
    n->b = nullptr;
    if (pr->result)
        n->type = expr_ktype[symb_var(&S->symb, pr->result)->info & (SK_DBL | VK_MASK)];
    if (form == ARGS_BARE)
        args(S, n, pr);
    else if (form == ARGS_PAREN && *S->comp.p == '(') {
        S->comp.p++;
        args(S, n, pr);
        comp_close(S);
    }
    return n;
}

Node *ref_undef(Session *const S)
{
    const uint16_t id = comp_var(S, 1, 0);
    if (*S->comp.p == '(') {
        S->comp.p++;
        ref_dropargs(S, true);
        comp_close(S);
    }
    Node *const n = ast_new(S, N_CALL);
    n->mode = CALL_UNDEF;
    n->id = id;
    n->b = nullptr;
    n->type = expr_ktype[symb_var(&S->symb, id)->info & (SK_DBL | VK_MASK)];
    return n;
}
