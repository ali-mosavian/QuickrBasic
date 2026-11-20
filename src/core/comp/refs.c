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
#include "comp/fold.h"
#include "comp/link.h"
#include "comp/sema.h"
#include "io/msg.h"
#include "io/repl.h"
#include "mem/array.h"
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
static uint32_t nrec(Comp *const C, const uint32_t name, const bool array)
{
    uint32_t id;

    if (!(symb_name(C->symb, name)->flags & NM_REC))
        return 0;
    if (comp_param(C, name, VI_ARR, &id)) { /* a record parameter, either kind */
        const uint32_t t = symb_var(C->symb, id)->type;
        return t >= ET_REC ? t - ET_REC : 0;
    }

    if (!array && comp_param(C, name, 0, &id))
        return 0; /* a parameter, not a record */

    return scop_rec(C->scop, name, array);
}

static uint32_t recvar(Comp *const C, const uint32_t name, const uint32_t t, const uint8_t kind)
{
    comp_scope(C, name);
    return symb_find_var(C->symb, name, ET_REC + t, kind, C->scope, C->isstatic);
}

uint32_t ref_recarray(Comp *const C, const uint32_t name, const uint32_t t)
{
    return recvar(C, name, t, VI_ARR);
}

uint32_t ref_arrayof(Comp *const C, const uint32_t id)
{
    const uint32_t name = symb_var(C->symb, id)->name;
    const uint32_t t = scop_rec(C->scop, name, true); /* a record array: its own variable */
    return t ? ref_recarray(C, name, t) : id;
}

/* rc_whole: all of a record of TYPE t, at off. */
static RecRef whole(Comp *const C, const uint32_t t, const uint16_t off)
{
    return (RecRef){.et = ET_REC + t, .off = off, .fix = decl_type(C->decl, t)->size};
}

/* A STRING field's, ours: a Desc, not STRING * n's chars. */
static bool desc_field(const RecRef *const r) { return r->et == ET_SD && !r->fix; }

/* A record's place: its bytes. */
static uint32_t field_size(const RecRef *const r)
{
    if (desc_field(r))
        return DSZ;

    return r->et >= ET_REC || r->et == ET_SD ? r->fix : decl_etsize[r->et];
}

/* Field f's place at off; a fixed array's, its element's. */
static RecRef fieldref(Comp *const C, const Field *const f, const uint32_t off)
{
    if (f->sub) /* a record in it */
        return whole(C, f->sub, (uint16_t)off);

    return (RecRef){.et = f->et, .off = (uint16_t)off, .fix = f->fix};
}

/* Field name of TYPE t, its text n letters. Syntax error: none. */
static const Field *field(
    Comp *const C,
    const uint32_t t,
    const uint8_t *const text,
    const uint32_t n
)
{
    const uint32_t name = symb_find_name(C->symb, text, (uint8_t)n);
    const TypeDef *const td = decl_type(C->decl, t);

    for (uint32_t i = 0; i < td->nfld; i++)
        if (td->fields[i].name == name)
            return &td->fields[i];

    synerr(C);
}

/* Where at's next index step goes: the end of its list (at.a). */
static Node **steps_end(Node *const at)
{
    Node **p = &at->a;

    while (*p)
        p = &(*p)->next;

    return p;
}

/* Node n at *end, the list's end past it. */
static void step(Node ***const end, Node *const n)
{
    **end = n;
    *end = &n->next;
}

static Node *step_op(Comp *const C, Node ***const end, const uint8_t op)
{
    Node *const n = ast_new(C->ast, N_OP);
    n->op = op;
    step(end, n);
    return n;
}

/* An element's ( s {, s} ) at comp.p, n's arguments: comp.whole not theirs, a field in them its
   element. */
static void subs_args(Comp *const C, Node *const n)
{
    const uint8_t whole = C->whole;
    C->whole = 0;
    expr_args(C, n);
    C->whole = whole;
}

/* The ( s {, s} ) at comp.p, ours: each checked as a number; the first, linked, their count *n. */
static Node *subscripts(Comp *const C, uint32_t *const n)
{
    Node *const subs = ast_new(C->ast, N_ELEM);
    subs_args(C, subs);
    const uint8_t *const past = C->p; /* sema_tree moves it to an operator's end */
    uint32_t k = 0;

    for (Node *s = subs->b; s; s = s->next, k++) {
        sema_tree(C, s);

        if (ast_type(s) & T_STR)
            mismatch(C);
    }

    C->p = past;
    *n = k;
    return subs->b;
}

/* An element of fixed array field f, ours, its ( s {, s} ) at comp.p, the first subscript the
   fastest: its offset in f by the constant subscripts; for the others, at's index steps ( addr s --
   addr ) FX.IDX lb count stride. Subscript out of range: a constant past its bounds, a count not
   f's. */
static uint32_t element(Comp *const C, Node *const at, const Field *const f)
{
    const uint8_t *const open = C->p;
    uint32_t n;
    Node *s = subscripts(C, &n);
    Node **end = steps_end(at);
    const RecRef e = fieldref(C, f, 0);
    uint64_t off = 0, stride = field_size(&e);

    if (n != f->ndim) {
        C->p = open;
        fault_error(C->fault, E_BS);
    }

    for (uint32_t k = 0; k < n; k++) {
        Node *const next = s->next;
        s->next = nullptr;
        const FieldDim *const d = decl_fielddim(C->decl, f, k);
        Cell v;

        if (ast_type(s) == T_INT && fold_number(C, s, &v)) {
            const int64_t i = (int64_t)v.i - d->lb;

            if (i < 0 || i >= d->count) {
                C->p = open;
                fault_error(C->fault, E_BS);
            }

            off += (uint64_t)i * stride;
        } else {
            step(&end, s);
            Node *const x = step_op(C, &end, O_FX_IDX);
            comp_dword(x, (uint32_t)d->lb);
            comp_dword(x, d->count);
            comp_dword(x, (uint32_t)stride);
        }

        stride *= d->count;
        s = next;
    }

    return (uint32_t)off;
}

/* An element of dynamic array field f, ours, its cell off bytes into at's place: its ( s {, s} ) at
   comp.p, then at's index step FE.ADDR off n esize ( addr s.. -- addr ). */
static void dyn_element(Comp *const C, Node *const at, const Field *const f, const uint32_t off)
{
    uint32_t n;
    Node *s = subscripts(C, &n);

    Node **end = steps_end(at);

    while (s) {
        Node *const next = s->next;
        s->next = nullptr;
        step(&end, s);
        s = next;
    }

    Node *const e = step_op(C, &end, O_FE_ADDR);
    comp_word(e, (uint16_t)off);
    comp_byte(e, (uint8_t)n);
    comp_word(e, (uint16_t)arry_et_size(C->decl, decl_fieldtype(f)));
}

/* An array field whole at comp.p, ours: f() or f(:) (stepped past) or f alone, not f(i); with
   WH_PAREN f(..), no .g after them. *wr how it is written. */
static bool field_whole(Comp *const C, Written *const wr)
{
    *wr = C->p[0] == TK_X && C->p[1] == TK1_WHOLE ? WR_ELEMS
          : C->p[0] == '(' && C->p[1] == ')'      ? WR_ARRAY
                                                  : WR_ALONE;

    if (*wr != WR_ALONE) {
        C->p += 2;
        return true;
    }

    return C->p[0] != '(' || (C->whole & WH_PAREN && *decl_paren(C->p) != '.');
}

/* Field f of TYPE t whole at off, ours, written wr: a dynamic one's cell, a fixed one's place. */
static RecRef field_array(Comp *const C, const uint32_t t, const Field *const f, const uint32_t off,
                          const Written wr)
{
    const uint32_t i = (uint32_t)(f - decl_type(C->decl, t)->fields);

    if (!f->dyn && t > FIELD_KEY_TYPES)
        fault_error(C->fault, E_OM);

    return (RecRef){.et = decl_fieldtype(f), .off = (uint16_t)off, .arr = true, .wr = wr,
                    .key = f->dyn ? 0 : decl_fieldkey(t, i)};
}

/* rc_path: the place of fields f.g.. of TYPE t, at's: the text len long, then at comp.p an array
   field's subscripts and .h.. after them, ours. An array field whole (a fixed one's with WH_FIXED):
   field_array's. */
static RecRef path(Comp *const C, Node *const at, uint32_t t, const uint8_t *text, uint32_t len)
{
    uint32_t off = 0;
    const Field *f;

    for (;;) {
        if (text[len - 1] < '.') /* a suffix is not the name's */
            len--;

        for (;;) {
            uint32_t n = 0;

            while (n < len && text[n] != '.')
                n++;

            f = field(C, t, text, n);
            off += f->off;
            text += n;
            len -= n;

            if (!len)
                break;
            if (!f->sub || f->ndim || f->dyn) /* .g: a field of a TYPE's */
                synerr(C);

            t = f->sub;
            text++;
            len--;
        }

        Written wr;

        if ((f->dyn || (f->ndim && C->whole & WH_FIXED)) && field_whole(C, &wr))
            return field_array(C, t, f, off, wr);

        if (f->dyn) {
            dyn_element(C, at, f, off);
            off = 0; /* from the element */
        } else if (f->ndim) {
            if (*C->p != '(') /* a fixed array field whole */
                synerr(C);

            off += element(C, at, f);
        } else
            break;

        if (!f->sub || C->p[0] != '.' || C->p[1] != M_VAR) /* (i).h */
            break;

        t = f->sub;
        text = C->p + 3;
        len = C->p[2];
        C->p = decl_item(C->p + 1);
    }

    return fieldref(C, f, off);
}

/* An N_REF of op and variable id; an element's subscripts follow at comp.p. */
static Node *place(Comp *const C, const uint8_t op, const uint32_t id)
{
    Node *const n = ast_new(C->ast, N_REF);
    n->op = op;
    n->id = id;
    n->b = nullptr;
    n->type = T_INT;

    if (op == O_RA_ADDR)
        subs_args(C, n);

    return n;
}

/* A call at comp.p of a FUNCTION whose result is a record, ours: the record its result goes to,
   or its field .f. Null: no such call. An array result: Type mismatch. */
static Node *reccall(Comp *const C)
{
    const uint32_t name = comp_nameof(C->p);
    const Name *const nm = symb_name(C->symb, name);

    if (!(nm->flags & NM_FUNC))
        return nullptr;

    Node *const n = ref_result(C);

    if (n == nullptr)
        return nullptr;
    if (n->rec.et < ET_REC)
        mismatch(C);
    if (C->p[0] == '.' && C->p[1] == M_VAR) { /* .f */
        const uint8_t *const f = ++C->p;
        C->p = decl_item(C->p);
        n->rec = path(C, n->a, n->rec.et - ET_REC, f + 2, f[1]);
    }

    return n;
}

/* ref_record's, a dynamic array field whole too (rec.arr). */
static Node *record(Comp *const C)
{
    Node *const call = reccall(C);

    if (call != nullptr)
        return call;

    const uint8_t *const p = C->p;
    const uint8_t len = p[1];
    const uint32_t name = comp_nameof(p);
    const uint8_t *const after = comp_skipname(p);
    uint32_t t;

    if (*after == '(' && (t = nrec(C, name, true))) { /* a(i) */
        C->p = after;
        Node *const n = place(C, O_RA_ADDR, ref_recarray(C, name, t));

        if (C->p[0] == '.' && C->p[1] == M_VAR) { /* .f */
            const uint8_t *const f = ++C->p;
            C->p = decl_item(C->p);
            n->rec = path(C, n, t, f + 2, f[1]);
        } else
            n->rec = whole(C, t, 0);

        return n;
    }

    if (*after != '(' && (t = nrec(C, name, false))) { /* r */
        C->p = after;
        Node *const n = place(C, O_R_ADDR, recvar(C, name, t, VI_RECARR));
        n->rec = whole(C, t, 0);
        return n;
    }

    const uint8_t *const text = p + 2; /* r.f, r.f(i) ours: the longest prefix a record's */
    for (uint32_t i = len - 1; (int32_t)i > 0; i--) {
        if (text[i] != '.')
            continue;

        const uint32_t pre = symb_find_name(C->symb, text, (uint8_t)i);
        t = nrec(C, pre, false);

        if (!t)
            continue;

        Node *const n = place(C, O_R_ADDR, recvar(C, pre, t, VI_RECARR));
        C->p = after;
        n->rec = path(C, n, t, text + i + 1, len - i - 1);
        return n;
    }

    return nullptr;
}

Node *ref_record(Comp *const C)
{
    Node *const r = record(C);

    if (r != nullptr && r->rec.arr) /* an array, not a value */
        mismatch(C);

    return r;
}

/* The name at comp.p a FUNCTION's whose result is an array, ours. */
static bool arrayresult(Comp *const C)
{
    const uint32_t name = comp_nameof(C->p);
    const Binding *const b =
        symb_name(C->symb, name)->flags & NM_FUNC ? scop_get(C->scop, name, name) : nullptr;

    return b && (b->et[1] || b->rec[1]);
}

/* At p a name without a dot, no .f after its (..): no field. */
static bool nofield(const uint8_t *const p)
{
    const uint8_t *const after = comp_skipname(p);
    return !memchr(p + 2, '.', p[1]) && (*after != '(' || *decl_paren(after) != '.');
}

Node *ref_fieldarray(Comp *const C, uint32_t *const type, Written *const wr)
{
    const uint8_t *const p = C->p;
    Node *const r = comp_cur(C) == M_VAR && !arrayresult(C) && !nofield(p) ? record(C) : nullptr;

    if (r != nullptr && r->rec.arr) {
        if (!wr && r->rec.wr == WR_ELEMS)
            synerr(C);
        if (wr)
            *wr = r->rec.wr;

        *type = r->rec.et;
        return expr_addr(C, r, 0);
    }

    C->p = p;
    return nullptr;
}

Node *ref_arrayfield(Comp *const C, uint32_t *const type, const uint8_t whole)
{
    const uint8_t outer = C->whole; /* a subscript's: 0 */
    C->whole = whole;
    Node *const a = ref_fieldarray(C, type, nullptr);
    C->whole = outer;
    return a;
}

Node *ref_field(Comp *const C, Node *const ref)
{
    const uint32_t et = ref->rec.et;

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
static uint32_t vakind(Comp *const C, const uint32_t id)
{
    const Var *const v = symb_var(C->symb, id);

    if (v->info & SK_DBL)
        return va_pack(VA_RAW, 8);
    switch (v->info & VK_MASK) {
    case VK_INT: /* 2 bytes but a LONG's; a BYTE's its cell's first; a SHORT element's 2 */
        if (v->type == ET_BY)
            return va_pack(VA_RAW, 1);
        if (v->type == ET_SH && v->info & VI_ARR)
            return va_pack(VA_RAW, 2);

        return va_pack(v->type == ET_I4 ? VA_RAW : VA_I2, 4);

    case VK_STR:
        return va_pack(VA_DESC, scop_fix(C->scop, v->name, v->info & VI_ARR));

    default:
        return va_pack(VA_RAW, 4);
    }
}

Node *ref_place(Comp *const C, uint32_t *const vk)
{
    if (comp_cur(C) != M_VAR)
        synerr(C);

    Node *const r = ref_record(C);

    if (r != nullptr) {
        const RecRef *const rec = &r->rec;
        *vk = desc_field(rec) ? va_pack(VA_DESC, 0)
                              : va_pack(rec->et == ET_I2 ? VA_I2 : VA_RAW, field_size(rec));
        return r;
    }

    const uint32_t id = comp_var(C, 0, 0);
    *vk = vakind(C, id);

    if (symb_var(C->symb, id)->info & VI_ARR) /* an element: RA.ADDR's */
        return place(C, O_RA_ADDR, id);
    if (comp_ispar(C, id)) /* a parameter's: its cell, the argument's */
        return place(C, O_V_ADDR, id);

    return place(C, O_V_CELL, id); /* its cell set at link: the cells move while linking */
}

Node *ref_bytes(Comp *const C, uint32_t *const vk)
{
    const uint8_t *const at = C->p;
    Node *const r = ref_place(C, vk);
    const uint32_t et = r->rec.et;

    if (et >= ET_REC && decl_dynamic(decl_type(C->decl, et - ET_REC))) { /* its bytes not all */
        C->p = at;
        C->dg = DG_TYPE_MISMATCH_RECORD;
        mismatch(C);
    }

    return r;
}

uint8_t ref_vtype(Comp *const C, const Node *const at)
{
    static const uint8_t byte[ET_BY + 1] = {0, 2, 20, 4, 8, 3, 2, 1}; /* by ET_* */
    uint32_t et = at->rec.et;

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
            found = comp_cur(C) == ')' && !desc_field(&r->rec);
        } else { /* a scalar: % 2, & ! 4, # 8 */
            const uint32_t id = comp_var(C, 0, 0);

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

const Proc *ref_proc(Comp *const C, uint32_t *const idx)
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

/* ct_aend: c ends an argument. */
static bool aend(const uint8_t c) { return !c || c == ',' || c == ')' || c == ':' || c == TK_REM; }

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
        else if (*p == M_VAR && aend(*after) && nrec(C, comp_nameof(p), false))
            C->p = after; /* a record whole: no value, its parameter's type unknown till linked */
        else
            expr_arg(C);
        if (*C->p != ',')
            break;

        C->p++;
    }
}

/* ct_ptm: variable id parameter par's type; else Parameter type mismatch, at the argument arg. */
static void ptm(Comp *const C, const uint32_t id, const uint32_t par, const uint8_t *const arg)
{
    if (symb_var(C->symb, id)->type != symb_var(C->symb, par)->type) {
        C->p = arg;
        fault_error(C->fault, E_PTM);
    }
}

static Node *argnode(Comp *const C, const uint8_t pass, Node *const a, const uint32_t id)
{
    Node *const n = ast_new(C->ast, N_ARG);
    n->op = pass;
    n->a = a;
    n->id = id;
    return n;
}

Node *ref_cell(Comp *const C, const uint32_t id)
{
    return place(C, symb_is_ref(C->symb, id) ? O_V_REF : O_V_CELL, id);
}

/* An array parameter's argument: n() an array whole. */
uint32_t ref_wholearray(Comp *const C, Written *const wr)
{
    if (comp_cur(C) != M_VAR)
        synerr(C);

    const uint8_t *const after = comp_skipname(C->p);
    const bool elems = after[0] == TK_X && after[1] == TK1_WHOLE;

    if (*after != '(' && !(elems && wr))
        synerr(C);

    const uint32_t id = ref_arrayof(C, comp_var(C, 2, 0));

    if (!(symb_var(C->symb, id)->info & VI_ARR))
        synerr(C);
    if (wr)
        *wr = elems ? WR_ELEMS : WR_ARRAY;
    if (elems) {
        C->p += 2;
        return id;
    }

    comp_open(C);
    comp_close(C);
    return id;
}

uint32_t ref_arrayvalue(Comp *const C, Node **const call, Written *const wr)
{
    *call = comp_cur(C) == M_VAR ? ref_result(C) : nullptr;

    if (*call && wr)
        *wr = WR_ARRAY;

    return *call ? (*call)->a->id : ref_wholearray(C, wr);
}

static Node *arrayarg(Comp *const C, const uint32_t par)
{
    const uint8_t *const arg = C->p;
    uint32_t type;
    Node *const f = ref_fieldarray(C, &type, nullptr); /* m.a(), ours */

    if (f != nullptr) {
        if (type != symb_var(C->symb, par)->type) {
            C->p = arg;
            fault_error(C->fault, E_PTM);
        }

        return argnode(C, PA_ARRAY, f, par);
    }

    Node *call;
    const uint32_t id = ref_arrayvalue(C, &call, nullptr);

    if (symb_is_record(C->symb, id)) {
        C->p = arg;
        fault_error(C->fault, E_PTM);
    }

    ptm(C, id, par, arg);
    link_given(C, id); /* the callee's REDIM's */
    return argnode(C, PA_ARRAY, call ? call : ref_cell(C, id), id);
}

/* A record parameter's argument, by P.REF: a record, an element of a record array, a record field,
   of its TYPE. */
static Node *recarg(Comp *const C, const uint32_t par)
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
static Node *arg(Comp *const C, const uint32_t par, const bool byval)
{
    const Var *const pv = symb_var(C->symb, par);

    if (symb_is_record(C->symb, par))
        return recarg(C, par);
    if (pv->info & VI_ARR)
        return arrayarg(C, par);
    if (!byval && comp_cur(C) == M_VAR) {
        const uint32_t name = comp_nameof(C->p);

        if (!(symb_name(C->symb, name)->flags & (NM_SUB | NM_FUNC | NM_FN)) &&
            !scop_con(C->scop, name)) {
            const uint8_t *const p = C->p;
            const uint8_t *const after = comp_skipname(p);
            const uint32_t ptype = symb_var(C->symb, par)->type;
            Node *const r = ref_record(C);

            if (r != nullptr) { /* a field */
                const RecRef *const rec = &r->rec;

                if (!(rec->et == ET_SD && rec->fix) && rec->et != ET_SH && rec->et != ET_BY &&
                    rec->et < ET_REC && rec->et == ptype && aend(comp_cur(C)))
                    return argnode(C, PA_PREF, r, par);
            } else if (*after == '(') { /* an element */
                const uint32_t id = comp_var(C, 0, 0);
                const Var *const v = symb_var(C->symb, id);

                if (v->info & VI_ARR && !scop_fix(C->scop, v->name, true) && v->type == ptype) {
                    Node *const e = place(C, O_RA_ADDR, id);

                    if (aend(comp_cur(C)))
                        return argnode(
                            C,
                            ptype == ET_SH || ptype == ET_BY ? PA_NREF : PA_PREF,
                            e,
                            par
                        );
                }
            }

            C->p = p;

            if (aend(*after)) { /* a variable alone: its cell */
                const uint32_t id = comp_var(C, 0, 0);
                ptm(C, id, par, p);
                return argnode(C, PA_VAR, ref_cell(C, id), par);
            }
        }
    }

    return argnode(C, PA_VALUE, expr_arg(C), par);
}

Node *ref_recplace(Comp *const C, const uint32_t id)
{
    Node *const r = place(C, O_R_ADDR, id);
    r->rec = whole(C, symb_var(C->symb, id)->type - ET_REC, 0);
    return r;
}

/* The place of record or array id: a record's whole, an array's cell. */
static Node *resplace(Comp *const C, const uint32_t id)
{
    return symb_is_record(C->symb, id) ? ref_recplace(C, id) : ref_cell(C, id);
}

/* The argument for FUNCTION pr's record or array result, ours: a hidden variable of the caller's,
   one per call, keyed by its place in the program, by reference. */
static Node *resarg(Comp *const C, const Proc *const pr)
{
    const uint32_t par = blok_resvar(pr);
    const Var v = *symb_var(C->symb, par);
    const uint32_t name = comp_hidden(C, RES_NAME, 0, C->line + (uint32_t)(C->p - C->body));
    const bool stat = C->proc && blok_proc(C->blok, C->proc)->flags & PF_STAT;
    const uint32_t id = symb_find_var(C->symb, name, v.type, v.info, C->proc, stat);
    return symb_is_record(C->symb, par) ? argnode(C, PA_PREF, resplace(C, id), par)
                                        : argnode(C, PA_ARRAY, resplace(C, id), id);
}

/* The call at comp.p of FUNCTION pr, whose result is a record or an array: its value the place of
   what its result went to, a (codegen pushes it past the call). */
static Node *resultcall(Comp *const C, const Proc *const pr)
{
    const uint8_t *const p = C->p;
    uint32_t idx;
    ref_proc(C, &idx);
    Node *const call = ref_call(C, p, idx, pr, ARGS_PAREN);
    call->a = resplace(C, ast_last(call)->a->id);
    call->rec = call->a->rec;
    call->type = T_INT;
    return call;
}

/* A line checked alone: a call of a FUNCTION not linked yet, its result's type b's, the array's
   when arr: its arguments passed over; an N_CALL CALL_UNDEF, a the place of a variable of that
   type. */
static Node *resunlinked(Comp *const C, const Binding *const b, const bool arr)
{
    C->p = comp_skipname(C->p);

    if (*C->p == '(') {
        C->p++;
        ref_dropargs(C, true);
        comp_close(C);
    }

    const uint32_t t = b->rec[arr];
    const uint32_t et = t ? ET_REC + t : b->et[arr];
    const uint8_t kind = !arr ? VI_RECARR : VI_ARR | (t ? 0 : comp_etkind[et]);
    const uint32_t name = comp_hidden(C, RES_NAME, 0, 0);
    Node *const n = ast_new(C->ast, N_CALL);
    n->mode = CALL_UNDEF;
    n->id = symb_find_var(C->symb, name, et, kind, C->proc, false);
    n->a = resplace(C, n->id);
    n->rec = n->a->rec;
    n->type = T_INT;
    return n;
}

Node *ref_result(Comp *const C)
{
    const uint32_t name = comp_nameof(C->p);
    const Name *const nm = symb_name(C->symb, name);
    const Binding *const b = scop_get(C->scop, name, name); /* its own scope's: its result's type */

    if (!(nm->flags & NM_FUNC) || !b)
        return nullptr;

    const bool arr = arrayresult(C);

    if (!arr && !b->rec[0]) /* a number's or a string's */
        return nullptr;
    if ((nm->proc ? nm->proc == C->proc : scop_own(C->scop) == name) &&
        (*comp_skipname(C->p) != '(' || arr))
        return nullptr; /* in it: its result, or an element of an array result */
    if (nm->proc)
        return resultcall(C, blok_proc(C->blok, nm->proc));

    return C->check ? resunlinked(C, b, arr) : nullptr;
}

/* ct_args: for each of the procedure's parameters, its argument, to call n. */
static void args(Comp *const C, Node *const n, const Proc *const pr)
{
    const bool def = pr->flags & PF_DEF; /* a DEF FN's: by value */
    Node *last = nullptr;

    for (uint32_t i = 0; i < blok_nargs(pr); i++) {
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
static bool is_variable(const Comp *const C, const uint32_t name)
{
    uint32_t id, scope;
    const Binding *const b = scop_look(C->scop, name, &scope);
    return comp_param((Comp *)C, name, 0, &id) || comp_param((Comp *)C, name, VI_ARR, &id) ||
           (b && (b->flags || b->et[0] || b->et[1]));
}

Node *ref_call(
    Comp *const C,
    const uint8_t *const name,
    const uint32_t idx,
    const Proc *const pr,
    const ArgsForm form
)
{
    Node *const n = ast_new(C->ast, N_CALL);
    n->num = idx;
    n->b = nullptr;

    if (pr->result)
        n->type = expr_ktype[symb_var(C->symb, pr->result)->info & (SK_DBL | VK_MASK)];
    if (form == ARGS_BARE)
        args(C, n, pr);
    else if (form == ARGS_PAREN && *C->p == '(') {
        C->p++;
        args(C, n, pr);
        comp_close(C);
    } else if (blok_nargs(pr)) { /* its frame would read cells the caller never pushed */
        C->p = name;

        if (is_variable(C, comp_nameof(name))) {
            C->dg = DG_DUPLICATE_DEFINITION_FUNCTION;
            fault_error(C->fault, E_RD);
        }

        fault_error(C->fault, E_ACM);
    }

    if (pr->flags & PF_RESARG) { /* ours: the record its result goes to, the last argument */
        Node *last = ast_last(n);
        ast_link(n, &last, resarg(C, pr));
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

    const uint32_t id = comp_var(C, 1, 0);

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
