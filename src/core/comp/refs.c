/* Parses references into the tree: a record's field or all of a record, a variable's place, a call and its arguments.
 *
 * # How it works
 * Names are resolved and nodes built here; nothing is emitted (codegen.c does). A record reference
 * (ref_record) is an N_REF with its place in the record (RecRef: the field's ET_*, offset, size);
 * ref_field makes its value, an N_FIELD. ref_place gives a variable's place for GET, PUT and FIELD.
 *
 * A call (ref_call) is an N_CALL whose arguments are N_ARG children, each passed by value, by
 * reference, as an array or as a record whole (PA_*); how they are written is an ArgsForm. A
 * FUNCTION DECLAREd only is Function not defined; in a line checked alone, an N_CALL CALL_UNDEF.
 *
 * # asm structure (retired/asm32/comp/ctl.asm, expr.asm)
 * | asm | here | |
 * |---|---|---|
 * | rc_ref, rc_load, rc_avar | ref_record, ref_field, ref_recarray | MIRROR |
 * | rc_nrec, rc_nreca, rc_saddr, rc_whole, rc_path, rc_svar | nrec(name, array), place, whole, path, recvar | MIRROR |
 * | cm_addr | ref_place | MIRROR |
 * | ct_proc, ct_fcall, ct_uarg | ref_proc(&idx), ref_call, ref_dropargs | MIRROR; D1 |
 * | ct_args, ct_arg, ct_aend, ct_ptm | args, arg (an N_ARG), argnode, ptm | MIRROR; D1 |
 * | g_vakind | vakind | MIRROR |
 * | g_rfet, g_rfoff, g_rffix | RecRef.et, off, fix | D2 |
 *
 * - D1. The asm emits a call's code as it parses; the C builds the N_CALL and codegen.c emits it.
 * - D2. A record's place is a RecRef on the node, not globals.
 */
#include "comp/refs.h"
#include "comp/expr.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "io/msg.h"
#include "io/repl.h"
#include "mem/scope.h"
#include "mem/symbols.h"

[[noreturn]]
static void synerr(Comp *const C)
{
    fault_error(C->fault, E_SN);
}
[[noreturn]]
static void mismatch(Comp *const C)
{
    fault_error(C->fault, E_TM);
}

/* ---- records */

/* rc_nrec, rc_nreca: the TYPE of record name, of its array; 0 none. A parameter of the procedure
   compiled is its own, else as DIM declared the name. */
static uint8_t nrec(Comp *const C, const uint16_t name, const bool array)
{
    uint16_t id;

    if (comp_param(C, name, VI_ARR, &id)) { /* a record parameter, either kind */
        const uint8_t t = symb_var(C->symb, id)->type;
        return t >= ET_REC ? t - ET_REC : 0;
    }

    if (!array && comp_param(C, name, 0, &id))
        return 0; /* a parameter, not a record */

    return scop_rec(C->scop, name, array);
}

static uint16_t recvar(Comp *const C, const uint16_t name, const uint8_t t, const uint8_t kind)
{
    comp_scope(C, name);
    return symb_find_var(C->symb, name, ET_REC + t, kind, C->scope, C->isstatic);
}

uint16_t ref_recarray(Comp *const C, const uint16_t name, const uint8_t t)
{
    return recvar(C, name, t, VI_ARR);
}

/* rc_whole: all of a record of TYPE t, at off. */
static RecRef whole(Comp *const C, const uint8_t t, const uint16_t off)
{
    return (RecRef){.et = ET_REC + t, .off = off, .fix = decl_type(C->decl, t)->size};
}

/* rc_path: the fields f.g.. of TYPE t, the text len long. */
static RecRef path(Comp *const C, uint8_t t, const uint8_t *text, uint32_t len)
{
    uint16_t off = 0;

    if (text[len - 1] < '.') /* a suffix is not the name's */
        len--;

    const Field *f;

    for (;;) {
        uint32_t n = 0;

        while (n < len && text[n] != '.')
            n++;

        const uint16_t name = symb_find_name(C->symb, text, (uint8_t)n);
        const TypeDef *const td = decl_type(C->decl, t);
        f = nullptr;

        for (uint32_t i = 0; i < td->nfld && !f; i++)
            if (td->fields[i].name == name)
                f = &td->fields[i];
        if (!f)
            synerr(C);

        off += f->off;
        text += n;
        len -= n;

        if (!len)
            break;
        if (!f->sub) /* .g: a field of a TYPE's */
            synerr(C);

        t = f->sub;
        text++;
        len--;
    }

    if (f->sub) /* a record in it */
        return whole(C, f->sub, off);

    return (RecRef){.et = f->et, .off = off, .fix = f->fix};
}

/* An N_REF of op and variable id; an element's subscripts follow at comp.p. */
static Node *place(Comp *const C, const uint8_t op, const uint16_t id)
{
    Node *const n = ast_new(C->ast, N_REF);
    n->op = op;
    n->id = id;
    n->b = nullptr;
    n->type = T_INT;

    if (op == O_RA_ADDR)
        expr_args(C, n);

    return n;
}

Node *ref_record(Comp *const C)
{
    const uint8_t *const p = C->p;
    const uint8_t len = p[1];
    const uint16_t name = comp_nameof(p);
    const uint8_t *const after = comp_skipname(p);
    uint8_t t;

    if (*after == '(') { /* a(i) */
        t = nrec(C, name, true);

        if (!t)
            return nullptr;

        C->p = after;
        Node *const n = place(C, O_RA_ADDR, ref_recarray(C, name, t));

        if (C->p[0] == '.' && C->p[1] == M_VAR) { /* .f */
            C->p++;
            n->rec = path(C, t, C->p + 2, C->p[1]);
            C->p = decl_item(C->p);
        } else
            n->rec = whole(C, t, 0);

        return n;
    }

    t = nrec(C, name, false);

    if (t) { /* r */
        C->p = after;
        Node *const n = place(C, O_R_ADDR, recvar(C, name, t, VI_RECARR));
        n->rec = whole(C, t, 0);
        return n;
    }

    const uint8_t *const text = p + 2; /* r.f: the longest prefix a record's */
    for (uint32_t i = len - 1; (int32_t)i > 0; i--) {
        if (text[i] != '.')
            continue;

        const uint16_t pre = symb_find_name(C->symb, text, (uint8_t)i);
        t = nrec(C, pre, false);

        if (!t)
            continue;

        Node *const n = place(C, O_R_ADDR, recvar(C, pre, t, VI_RECARR));
        n->rec = path(C, t, text + i + 1, len - i - 1);
        C->p = after;
        return n;
    }

    return nullptr;
}

Node *ref_field(Comp *const C, Node *const ref)
{
    const uint8_t et = ref->rec.et;

    if (et >= ET_REC)
        mismatch(C);

    Node *const n = ast_new(C->ast, N_FIELD);
    n->a = ref;
    n->type = et == ET_SD ? T_STR : expr_ktype[comp_etkind[et]];
    n->sh = et == ET_SH ? SH_SHORT : SH_NONE;
    return n;
}

/* ---- places */

/* Variable id's place: its kind and bytes; a string's Desc and its STRING * n. */
static uint32_t vakind(Comp *const C, const uint16_t id)
{
    const Var *const v = symb_var(C->symb, id);

    if (v->info & SK_DBL)
        return va_pack(VA_RAW, 8);
    switch (v->info & VK_MASK) {
    case VK_INT: /* 2 bytes but a LONG's; a BYTE's its cell's first */
        if (v->type == ET_BY)
            return va_pack(VA_RAW, 1);

        return va_pack(v->type == ET_I4 ? VA_RAW : VA_I2, 4);

    case VK_STR:
        return va_pack(VA_DESC, scop_fix(C->scop, v->name, v->info & VI_ARR));

    default:
        return va_pack(VA_RAW, 4);
    }
}

/* A record's place: its bytes. */
static uint32_t field_size(const RecRef *const r)
{
    return r->et >= ET_REC || r->et == ET_SD ? r->fix : decl_etsize[r->et];
}

Node *ref_place(Comp *const C, uint32_t *const vk)
{
    if (comp_cur(C) != M_VAR)
        synerr(C);

    Node *const r = ref_record(C);

    if (r != nullptr) {
        const RecRef *const rec = &r->rec;
        *vk = va_pack(rec->et == ET_I2 ? VA_I2 : VA_RAW, field_size(rec));
        return r;
    }

    const uint16_t id = comp_var(C, 0, 0);
    *vk = vakind(C, id);

    if (symb_var(C->symb, id)->info & VI_ARR) /* an element: RA.ADDR's */
        return place(C, O_RA_ADDR, id);
    if (comp_ispar(C, id)) /* a parameter's: its cell, the argument's */
        return place(C, O_V_ADDR, id);

    return place(C, O_V_CELL, id); /* its cell set at link: the cells move while linking */
}

uint8_t ref_vtype(Comp *const C, const Node *const at)
{
    static const uint8_t byte[ET_BY + 1] = {0, 2, 20, 4, 8, 3, 2, 1}; /* by ET_* */
    uint8_t et = at->rec.et;

    if (!et) {
        const Var *const v = symb_var(C->symb, at->id);
        const uint8_t kind = v->info & VK_MASK;
        et = v->info & SK_DBL ? ET_R8
             : kind == VK_INT ? (v->type == ET_I4 ? ET_I4 : ET_I2)
             : kind == VK_STR ? ET_SD
                              : ET_R4;
    }

    if (et > ET_BY) /* a record */
        mismatch(C);

    return byte[et];
}

bool ref_len(Comp *const C, uint32_t *const size)
{
    const uint8_t *const p = C->p;
    C->p++;
    bool found = false;

    if (comp_cur(C) == '(' && comp_curn(C) == M_VAR) {
        Node *const r = ref_record(C);

        if (r != nullptr) {
            *size = field_size(&r->rec);
            found = comp_cur(C) == ')';
        } else { /* a scalar: % 2, & ! 4, # 8 */
            const uint16_t id = comp_var(C, 0, 0);

            if (!(symb_var(C->symb, id)->info & VI_ARR)) {
                const uint32_t v = vakind(C, id);
                *size = va_kind(v) == VA_I2 ? 2 : (uint8_t)v;
                found = comp_cur(C) == ')' && va_kind(v) != VA_DESC;
            }
        }
    } /* a size: the place's nodes not needed */
    if (found)
        C->p++;
    else
        C->p = p;

    return found;
}

/* ---- procedures */

const Proc *ref_proc(Comp *const C, uint8_t *const idx)
{
    const uint8_t *const p = C->p;

    if (*p != M_VAR)
        return nullptr;

    const Name *const n = symb_name(C->symb, comp_nameof(p));

    if (!(n->flags & (NM_SUB | NM_FUNC | NM_FN)) || !n->proc)
        return nullptr;

    C->p = comp_skipname(p);
    *idx = n->proc;
    return blok_proc(C->blok, n->proc);
}

void ref_dropargs(Comp *const C, const bool paren)
{
    for (;;) {
        if (paren ? *C->p == ')' : comp_end(C))
            break;
        if (*C->p == TK_BYVAL)
            C->p++;

        const uint8_t *const p = C->p;
        const uint8_t *const after = *p == M_VAR ? comp_skipname(p) : p;

        if (*p == M_VAR && after[0] == '(' && after[1] == ')')
            C->p = after + 2; /* a() an array whole */
        else
            expr_arg(C);
        if (*C->p != ',')
            break;

        C->p++;
    }
}

/* ct_aend: c ends an argument. */
static bool aend(const uint8_t c) { return !c || c == ',' || c == ')' || c == ':' || c == TK_REM; }

/* ct_ptm: variable id parameter par's type; else Parameter type mismatch, at the argument arg. */
static void ptm(Comp *const C, const uint16_t id, const uint16_t par, const uint8_t *const arg)
{
    if (symb_var(C->symb, id)->type != symb_var(C->symb, par)->type) {
        C->p = arg;
        fault_error(C->fault, E_PTM);
    }
}

static Node *argnode(Comp *const C, const uint8_t pass, Node *const a, const uint16_t id)
{
    Node *const n = ast_new(C->ast, N_ARG);
    n->op = pass;
    n->a = a;
    n->id = id;
    return n;
}

/* Variable id's cell, an argument's ref: a parameter's is its argument's. */
static Node *cell(Comp *const C, const uint16_t id)
{
    return place(C, symb_is_ref(C->symb, id) ? O_V_ADDR : O_V_CELL, id);
}

/* An array parameter's argument: n() an array whole. */
static Node *arrayarg(Comp *const C, const uint16_t par)
{
    const uint8_t *const arg = C->p;

    if (comp_cur(C) != M_VAR)
        synerr(C);

    const uint16_t name = comp_nameof(C->p);
    const uint8_t *const after = comp_skipname(C->p);

    if (*after != '(')
        synerr(C);

    const uint8_t t = scop_rec(C->scop, name, true);
    uint16_t id;

    if (t) {
        C->p = after;
        id = ref_recarray(C, name, t);
    } else
        id = comp_var(C, 0, 0);

    if (!(symb_var(C->symb, id)->info & VI_ARR))
        synerr(C);

    ptm(C, id, par, arg);
    comp_open(C);
    comp_close(C);
    return argnode(C, PA_ARRAY, cell(C, id), id);
}

/* A record parameter's argument, by P.REF: a record, an element of a record array, a record field,
   of its TYPE. */
static Node *recarg(Comp *const C, const uint16_t par)
{
    const uint8_t *const arg = C->p;
    Node *const r = comp_cur(C) == M_VAR ? ref_record(C) : nullptr;

    if (r == nullptr || !aend(comp_cur(C)))
        synerr(C);
    if (r->rec.et != symb_var(C->symb, par)->type) {
        C->p = arg;
        fault_error(C->fault, E_PTM);
    }

    return argnode(C, PA_PREF, r, par);
}

/* ct_arg: an argument to parameter par: a variable alone by reference, an element or a record's
   number field by P.REF, else the value. A CONST, BYVAL's, a DEF FN's: the value. */
static Node *arg(Comp *const C, const uint16_t par, const bool byval)
{
    const Var *const pv = symb_var(C->symb, par);

    if (pv->type >= ET_REC && (pv->info & (VI_ARR | VK_MASK)) == VI_RECARR)
        return recarg(C, par);
    if (pv->info & VI_ARR)
        return arrayarg(C, par);
    if (!byval && comp_cur(C) == M_VAR) {
        const uint16_t name = comp_nameof(C->p);

        if (!(symb_name(C->symb, name)->flags & (NM_SUB | NM_FUNC | NM_FN)) &&
            !scop_con(C->scop, name)) {
            const uint8_t *const p = C->p;
            const uint8_t *const after = comp_skipname(p);
            const uint8_t ptype = symb_var(C->symb, par)->type;
            Node *const r = ref_record(C);

            if (r != nullptr) { /* a field */
                const RecRef *const rec = &r->rec;

                if (!(rec->et == ET_SD && rec->fix) && rec->et != ET_SH && rec->et != ET_BY &&
                    rec->et < ET_REC && rec->et == ptype && aend(comp_cur(C)))
                    return argnode(C, PA_PREF, r, par);
            } else if (*after == '(') { /* an element */
                const uint16_t id = comp_var(C, 0, 0);
                const Var *const v = symb_var(C->symb, id);

                if (v->info & VI_ARR && !scop_fix(C->scop, v->name, true) && v->type == ptype) {
                    Node *const e = place(C, O_RA_ADDR, id);

                    if (aend(comp_cur(C)))
                        return argnode(C, PA_PREF, e, par);
                }
            }

            C->p = p;

            if (aend(*after)) { /* a variable alone: its cell */
                const uint16_t id = comp_var(C, 0, 0);
                ptm(C, id, par, p);
                return argnode(C, PA_VAR, cell(C, id), par);
            }
        }
    }

    return argnode(C, PA_VALUE, expr_arg(C), par);
}

/* ct_args: for each of the procedure's parameters, its argument, to call n. */
static void args(Comp *const C, Node *const n, const Proc *const pr)
{
    const bool def = pr->flags & PF_DEF; /* a DEF FN's: by value */
    Node *last = nullptr;

    for (uint32_t i = 0; i < pr->npar; i++) {
        if (i) {
            if (comp_cur(C) != ',')
                synerr(C);

            C->p++;
        }

        const bool byval = pr->params[i].byval || def;
        ast_link(n, &last, arg(C, pr->params[i].var, byval));
    }
}

/* Name a parameter of the procedure being compiled or a declared variable: a FUNCTION's name
   meant as the variable. */
static bool is_variable(const Comp *const C, const uint16_t name)
{
    uint16_t id, scope;
    const Binding *const b = scop_look(C->scop, name, &scope);
    return comp_param((Comp *)C, name, 0, &id) || comp_param((Comp *)C, name, VI_ARR, &id) ||
           (b && (b->flags || b->et[0] || b->et[1]));
}

Node *ref_call(
    Comp *const C, const uint8_t *const name, const uint8_t idx, const Proc *const pr,
    const ArgsForm form
)
{
    Node *const n = ast_new(C->ast, N_CALL);
    n->op = idx;
    n->b = nullptr;

    if (pr->result)
        n->type = expr_ktype[symb_var(C->symb, pr->result)->info & (SK_DBL | VK_MASK)];
    if (form == ARGS_BARE)
        args(C, n, pr);
    else if (form == ARGS_PAREN && *C->p == '(') {
        C->p++;
        args(C, n, pr);
        comp_close(C);
    } else if (pr->npar) { /* its frame would read cells the caller never pushed */
        C->p = name;

        if (is_variable(C, comp_nameof(name))) {
            C->dg = DG_DUPLICATE_DEFINITION_FUNCTION;
            fault_error(C->fault, E_RD);
        }

        fault_error(C->fault, E_ACM);
    }

    return n;
}

Node *ref_undef(Comp *const C)
{
    if (!C->check) { /* checked alone: blok_procs has it only once linked */
        if (symb_name(C->symb, comp_nameof(C->p))->flags & NM_FUNC)
            C->dg = DG_FUNCTION_NOT_DEFINED_DECLARED;

        fault_error(C->fault, E_UF);
    }

    const uint16_t id = comp_var(C, 1, 0);

    if (*C->p == '(') {
        C->p++;
        ref_dropargs(C, true);
        comp_close(C);
    }

    Node *const n = ast_new(C->ast, N_CALL);
    n->mode = CALL_UNDEF;
    n->id = id;
    n->b = nullptr;
    n->type = expr_ktype[symb_var(C->symb, id)->info & (SK_DBL | VK_MASK)];
    return n;
}
