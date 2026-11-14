/* The code generator: a tree (comp/ast.h) to stack-machine code. The only module that writes an
   expression's or a reference's bytes. Every operand is pushed; an operator takes the cells it
   needs. A right operand that is one load folds into the operator: F.ADD.V, I.CMP.U8 ... */
#include "comp/gen.h"
#include "session.h"
#include "comp/ctrl.h"
#include "comp/dfa.h"
#include "comp/fns.h"
#include "io/repl.h"
#include "mem/symb.h"
#include "num/dubl.h"
#include "num/fmth.h"
#include "vm/flow.h"

[[noreturn]] static void mismatch(Session *const S) { repl_error(S, E_TM); }

static const uint8_t xop[] = {O_I_DIV, O_I_MOD, O_I_XOR, O_I_EQV, O_I_IMP}; /* from OP_IDIV */
static const uint8_t fcmpk[4] = {O_F_CMP_V, O_F_CMP_VI, O_F_CMP_U8, O_F_CMP_F32};
static const uint8_t lda1[5] = {O_F_LD_A1, O_I_LD_A1, O_S_LD_A1, O_D_LD_A1, O_D_LD_A1};
static const uint8_t ldan[5] = {O_F_LD_A, O_I_LD_A, O_S_LD_A, O_D_LD_A, O_D_LD_A};

static void gen(Session *S, uint16_t n);
static void loadvar(Session *S, uint16_t id);
static void gen_elem(Session *S, const Node *n);
static void gen_fn(Session *S, const Node *n);
static void gen_xfn(Session *S, const Node *n);
static void gen_ref(Session *S, const Node *n);
static void gen_field(Session *S, const Node *n);
static void gen_addr(Session *S, const Node *n);
static void gen_arg(Session *S, const Node *n);
static void gen_call(Session *S, const Node *n);

static Node *node(Session *const S, const uint16_t i) { return ast_node(S, i); }

uint8_t gen_shclass(Session *const S)
{
    S->gen.shclass = S->comp.out == S->gen.shend ? S->gen.sh : SH_NONE;
    return S->gen.shclass;
}

void gen_shortid(Session *const S, const uint16_t id)
{
    if (symb_var(&S->symb, id)->type == ET_SH) {
        S->gen.sh = SH_SHORT;
        S->gen.shend = S->comp.out;
    }
}

/* cm_sop: operator rel's result is a SHORT's when both operands are SHORTs or literals that fit,
   one a SHORT: + - * \ MOD, QBasic's INTEGER arithmetic. lsh, rsh: the operands' classes. */
static uint8_t shortop(const uint8_t rel, const uint8_t lsh, const uint8_t rsh)
{
    if (!lsh || !rsh || !((lsh | rsh) & SH_SHORT))
        return SH_NONE;
    const bool arith = rel <= OP_MUL || rel == OP_IDIV || rel == OP_MOD;
    return arith ? SH_SHORT : SH_NONE;
}

static bool isright(Session *const S, const uint8_t op, const uint32_t size, const uint8_t *const right)
{
    return (uint32_t)(S->comp.out - right) == size && *right == op;
}

uint8_t *gen_lfloat(Session *const S, uint8_t *const left, uint8_t *const right)
{
    if (left) {
        if (*left == O_I_LD_V) {
            *left = O_F_LD_VI;
            return right;
        }
        if (*left == O_I_LD_U8) {
            *left = O_F_LD_U8;
            return right;
        }
    }
    comp_insert(S, O_I_TOF, right);
    return right + 1;
}

void gen_icmp(Session *const S, const uint8_t rel, uint8_t *const right)
{
    S->comp.type = T_INT;
    const uint8_t m = rel & REL_MASK;
    uint8_t op;
    if (isright(S, O_I_LD_V, 3, right))
        op = O_I_CMP_V;
    else if (isright(S, O_I_LD_U8, 2, right))
        op = O_I_CMP_U8;
    else {
        S->comp.cmp = S->comp.out;
        comp_opb(S, O_I_CMP, m);
        S->comp.cmpend = S->comp.out;
        return;
    }
    S->comp.cmp = right;
    *right = op;
    comp_byte(S, m);
    S->comp.cmpend = S->comp.out;
}

/* The right operand's load that folds: the first op of its group, F_ADD_V .. F_ADD_F32; 0 none. */
static uint8_t fltkind(Session *const S, const uint8_t *const right)
{
    if (isright(S, O_F_LD_V, 3, right))
        return O_F_ADD_V;
    if (isright(S, O_F_LD_VI, 3, right))
        return O_F_ADD_VI;
    if (isright(S, O_F_LD_U8, 2, right))
        return O_F_ADD_U8;
    if (isright(S, O_F_LD_F32, 5, right))
        return O_F_ADD_F32;
    return 0;
}

void gen_fcmp(Session *const S, const uint8_t rel, uint8_t *const right)
{
    S->comp.type = T_INT;
    const uint8_t m = rel & REL_MASK;
    const uint8_t k = fltkind(S, right);
    if (k) {
        S->comp.cmp = right;
        *right = fcmpk[(k - O_F_ADD_V) >> 2];
        comp_byte(S, m);
    } else {
        S->comp.cmp = S->comp.out;
        comp_opb(S, O_F_CMP, m);
    }
    S->comp.cmpend = S->comp.out;
}

/* A leaf's value as one load of type t (T_INT or T_FLT): its op and operand in *op, b; false:
   not one load. left: a left operand's, which converts only I.LD.V and I.LD.U8 (cm_lflt). */
static bool one_load(Session *const S, const Node *const n, const uint8_t t, const bool left, uint8_t *const op,
                     uint8_t b[static 4])
{
    if (n->kind == N_VAR) {
        const uint16_t id = n->id;
        const uint8_t info = symb_var(&S->symb, id)->info;
        if (info & SK_DBL || comp_ispar(S, id))
            return false;
        store16(b, id);
        switch (info & VK_MASK) {
        case VK_INT: *op = t == T_INT ? O_I_LD_V : O_F_LD_VI; return true;
        case VK_FLT: *op = O_F_LD_V; return t == T_FLT;
        default: return false;
        }
    }
    if (n->kind != N_LIT)
        return false;
    for (uint32_t k = 0; k < 4; k++)
        b[k] = n->lit[k];
    if (n->type == T_FLT) {
        *op = n->op;
        return t == T_FLT;
    }
    if (n->type != T_INT)
        return false;
    if (t == T_INT || n->op == O_I_LD_U8) {
        *op = t == T_INT ? n->op : O_F_LD_U8;
        return true;
    }
    if (left) /* an I16, I32 literal: I.TOF after it */
        return false;
    const uint32_t v = n->op == O_I_LD_I32 ? load32(n->lit) : load16(n->lit);
    store32(b, n->op == O_I_LD_I32 ? fmth_from_i32((int32_t)v) : fmth_from_u16((uint16_t)v));
    *op = O_F_LD_F32;
    return true;
}

/* The operand bytes after a load op. */
static uint32_t operand_size(const uint8_t op)
{
    const uint8_t b[1] = {op};
    return flow_opsize(b) - 1;
}

/* Op, then load op lop's operand b: the load itself, or an operator its load folded into. */
static void emit_load(Session *const S, const uint8_t op, const uint8_t lop, const uint8_t b[static 4])
{
    comp_op(S, op);
    for (uint32_t k = 0; k < operand_size(lop); k++)
        comp_byte(S, b[k]);
}

/* Node i's value as type t, left a left operand's. */
static void gen_as(Session *const S, const uint16_t i, const uint8_t t, const bool left)
{
    uint8_t op, b[4];
    const Node *const n = node(S, i);
    if (t == T_FLT && ast_type(S, i) == T_INT && one_load(S, n, T_FLT, left, &op, b)) {
        S->gen.shend = nullptr;
        emit_load(S, op, op, b);
        S->comp.type = T_FLT;
        return;
    }
    gen(S, i);
    switch (t) {
    case T_INT: comp_toint(S); break;
    case T_DBL: comp_todouble(S); break;
    case T_FLT:
        if (left && S->comp.type == T_INT) { /* not the leaf rewritten: I.TOF after it */
            S->comp.type = T_FLT;
            comp_op(S, O_I_TOF);
        } else
            comp_tofloat(S);
        break;
    default: break;
    }
}

/* Leaf n's SHORT class: a SHORT variable's, a literal's. */
static uint8_t leaf_class(Session *const S, const Node *const n)
{
    if (n->kind == N_VAR)
        return symb_var(&S->symb, n->id)->type == ET_SH ? SH_SHORT : SH_NONE;
    return n->sh;
}

/* A relation's compare op (its right's load lop, operand b, folded in; b null none), mask m
   after it; comp.cmp marks it for a one-line IF. */
static void compare(Session *const S, const uint8_t op, const uint8_t lop, const uint8_t *const b, const uint8_t m)
{
    S->comp.cmp = S->comp.out;
    if (b)
        emit_load(S, op, lop, b);
    else
        comp_op(S, op);
    comp_byte(S, m);
    S->comp.cmpend = S->comp.out;
    S->comp.type = T_INT;
}

/* INT or FLOAT operator rel (+ - * / AND OR as 0..5, a relation REL | mask) after its left
   operand: the right one as type t, its one load folded into the op (.V, .VI, .U8, .F32). Its
   SHORT class to *rsh. */
static void numeric(Session *const S, const uint8_t rel, const uint16_t r, const uint8_t t, uint8_t *const rsh)
{
    const bool flt = t == T_FLT;
    uint8_t lop, b[4];
    if (one_load(S, node(S, r), t, false, &lop, b)) {
        *rsh = leaf_class(S, node(S, r));
        uint8_t form; /* the op group's index: .V, .VI, .U8, .F32 */
        switch (lop) {
        case O_I_LD_V:
        case O_F_LD_V: form = 0; break;
        case O_F_LD_VI: form = 1; break;
        case O_I_LD_U8:
        case O_F_LD_U8: form = 2; break;
        case O_F_LD_F32: form = 3; break;
        default: form = 0xFF; /* I16, I32, PI: no form */
        }
        if (form != 0xFF && (flt || form != 1)) {
            if (ast_isrel(rel)) {
                const uint8_t op = flt ? fcmpk[form] : form ? O_I_CMP_U8 : O_I_CMP_V;
                compare(S, op, lop, b, rel & REL_MASK);
            } else {
                const uint8_t base = flt ? (uint8_t[]){O_F_ADD_V, O_F_ADD_VI, O_F_ADD_U8, O_F_ADD_F32}[form]
                                         : form ? O_I_ADD_U8 : O_I_ADD_V;
                emit_load(S, base + rel, lop, b);
                S->comp.type = t;
            }
            return;
        }
    }
    gen_as(S, r, t, false);
    *rsh = gen_shclass(S);
    if (ast_isrel(rel))
        compare(S, flt ? O_F_CMP : O_I_CMP, 0, nullptr, rel & REL_MASK);
    else {
        comp_op(S, (flt ? O_F_ADD : O_I_ADD) + rel);
        S->comp.type = t;
    }
}

/* Binary node n: its operands as its work type, then the operator. */
static void gen_binary(Session *const S, const Node *const n)
{
    const uint8_t rel = n->op;
    const uint8_t w = ast_work(S, n);
    uint8_t lsh = SH_NONE, rsh = SH_NONE;
    if (w == W_STR) {
        gen(S, n->a);
        gen(S, n->b);
        S->comp.p = n->end;
        if (!(S->comp.type & T_STR))
            mismatch(S);
        if (rel == OP_ADD)
            comp_op(S, O_S_CAT);
        else if (!ast_isrel(rel))
            mismatch(S);
        else
            compare(S, O_S_CMP, 0, nullptr, rel & REL_MASK);
        S->gen.sh = SH_NONE;
        S->gen.shend = S->comp.out;
        return;
    }
    const uint8_t lt = w == W_INT ? T_INT : w == W_DBL ? T_DBL : T_FLT;
    gen_as(S, n->a, lt, true);
    lsh = gen_shclass(S);
    if (ast_type(S, n->b) & T_STR) {
        gen(S, n->b);
        S->comp.p = n->end;
        mismatch(S);
    }
    switch (w) {
    case W_DBL:
        gen_as(S, n->b, T_DBL, false);
        rsh = gen_shclass(S);
        S->comp.p = n->end;
        S->comp.iload = S->comp.iloadend = nullptr;
        if (rel == OP_POW)
            comp_op(S, O_D_POW);
        else if (ast_isrel(rel)) {
            comp_opb(S, O_D_CMP, rel & REL_MASK);
            S->comp.type = T_INT;
        } else
            comp_op(S, O_D_ADD + rel);
        break;
    case W_POW:
        gen_as(S, n->b, T_FLT, false);
        rsh = gen_shclass(S);
        S->comp.p = n->end;
        comp_call(S, K_FPOW);
        S->comp.type = T_FLT;
        break;
    default:
        if (ast_isxop(rel)) {
            gen_as(S, n->b, T_INT, false);
            rsh = gen_shclass(S);
            S->comp.p = n->end;
            comp_op(S, xop[rel - OP_IDIV]);
            S->comp.type = T_INT;
        } else {
            numeric(S, ast_islogic(rel) ? rel - 1 : rel, n->b, lt, &rsh); /* AND, OR: O_I_ADD's 4th, 5th */
            S->comp.p = n->end;
        }
    }
    const uint8_t sh = shortop(rel, lsh, rsh);
    if (sh && S->comp.type == T_INT) /* a SHORT's result checked */
        comp_op(S, O_I_CHK16);
    S->gen.sh = sh;
    S->gen.shend = S->comp.out;
}

/* Unary node n: - or NOT of its operand. */
static void gen_unary(Session *const S, const Node *const n)
{
    gen(S, n->a);
    S->comp.p = n->end;
    if (n->op == TK_NOT) {
        comp_toint(S);
        comp_op(S, O_I_NOT);
    } else if (S->comp.type & T_STR)
        mismatch(S);
    else if (S->comp.type == T_DBL)
        comp_op(S, O_D_NEG);
    else if (S->comp.type == T_FLT)
        comp_op(S, O_F_NEG);
    else {
        gen_shclass(S); /* a SHORT's negated is one */
        comp_op(S, O_I_NEG);
        S->gen.sh = S->gen.shclass;
        S->gen.shend = S->comp.out;
    }
}

/* The SHORT class of the code just made, as a leaf's. */
static void set_class(Session *const S, const uint8_t sh)
{
    if (sh) {
        S->gen.sh = sh;
        S->gen.shend = S->comp.out;
    }
}

/* Leaf n's code. */
static void gen_leaf(Session *const S, const Node *const n)
{
    switch (n->kind) {
    case N_LIT: {
        uint8_t *const at = S->comp.out;
        const uint8_t b[1] = {n->op};
        const uint32_t size = flow_opsize(b);
        comp_op(S, n->op);
        for (uint32_t i = 1; i < size; i++)
            comp_byte(S, n->lit[i - 1]);
        S->comp.type = n->type;
        if (n->type == T_INT) {
            S->comp.iload = at;
            S->comp.iloadend = S->comp.out;
        }
        break;
    }
    default: /* N_VAR */
        loadvar(S, n->id);
        return;
    }
    set_class(S, n->sh);
}

static void gen(Session *const S, const uint16_t i)
{
    const Node *const n = node(S, i);
    S->gen.shend = nullptr; /* not a SHORT's unless it says */
    switch (n->kind) {
    case N_UN: gen_unary(S, n); return;
    case N_BIN: gen_binary(S, n); return;
    case N_ELEM: gen_elem(S, n); return;
    case N_FN: gen_fn(S, n); break;
    case N_XFN: gen_xfn(S, n); break;
    case N_REF: gen_ref(S, n); break;
    case N_FIELD: gen_field(S, n); break;
    case N_ADDR: gen_addr(S, n); break;
    case N_ARG: gen_arg(S, n); break;
    case N_CALL: gen_call(S, n); break;
    default: gen_leaf(S, n);
    }
}

void gen_tree(Session *const S, const uint16_t n) { gen(S, n); }

/* A scalar's value, its id's: X.LD.V. */
static void loadvar(Session *const S, const uint16_t id)
{
    const uint8_t info = symb_var(&S->symb, id)->info;
    if (info & SK_DBL) {
        S->comp.type = T_DBL;
        comp_opid(S, O_D_LD_V, id);
        return;
    }
    const uint8_t k = info & VK_MASK;
    if (k == VK_INT) {
        S->comp.iload = S->comp.out;
        comp_opid(S, O_I_LD_V, id);
        S->comp.iloadend = S->comp.out;
        S->comp.type = T_INT;
        gen_shortid(S, id);
    } else if (k == VK_STR) {
        S->comp.type = T_STR;
        comp_opid(S, O_S_LD_V, id);
    } else {
        S->comp.type = T_FLT;
        comp_opid(S, O_F_LD_V, id);
    }
}

/* The value as kind k (an XFn's). */
static void xarg(Session *const S, const char k)
{
    switch (k) {
    case 'D':
        comp_todouble(S);
        return;
    case 'G': /* FIX's: a DOUBLE's D.FN 3, else CALL FIX */
        if (S->comp.type == T_DBL) {
            S->gen.fixdbl = true;
            comp_opb(S, O_D_FN, DF_FIX);
        } else {
            comp_tofloat(S);
            comp_call(S, K_FIX);
        }
        return;
    case 'W': /* HEX$, OCT$: a SHORT's 16 bits */
        gen_shclass(S);
        comp_toint(S);
        if (S->gen.shclass == SH_SHORT) {
            comp_int(S, 0xFFFF);
            comp_op(S, O_I_AND);
        }
        return;
    case 'I':
        comp_toint(S);
        return;
    case 'F':
        comp_tofloat(S);
        return;
    case 'S':
        comp_tostring(S);
        return;
    }
    if (S->comp.type & T_STR) /* X: a string or a number, then 1 or 0 */
        comp_int(S, 1);
    else {
        comp_toint(S);
        comp_int(S, 0);
    }
}

/* Function f's (TK_SGN on) argument converted; comp.type its result. */
static void farg(Session *const S, const uint8_t f)
{
    const uint8_t sig = fns_sig[f];
    switch (sig & FA_MASK) {
    case FA_F: comp_tofloat(S); break;
    case FA_A: comp_toaddr(S); break;
    case FA_S: comp_tostring(S); break;
    case FA_B: comp_tobyte(S); break;
    case FA_N: /* POS's: a number */
        if (S->comp.type & T_STR)
            mismatch(S);
        break;
    case FA_I: comp_toint(S); break;
    }
    const uint8_t res = sig & FR_MASK;
    S->comp.type = res == FR_S ? T_STR : res;
}

static void gen_elem(Session *const S, const Node *const n)
{
    for (uint16_t a = n->b; a != NO_NODE; a = node(S, a)->next)
        gen_as(S, a, T_INT, false);
    const uint16_t id = n->id;
    const uint8_t k = symb_var(&S->symb, id)->info & (SK_DBL | VK_MASK);
    S->comp.type = expr_ktype[k];
    S->comp.iload = S->comp.iloadend = nullptr;
    if (n->nargs == 1)
        comp_opid(S, lda1[k], id);
    else {
        comp_opid(S, ldan[k], id);
        comp_byte(S, n->nargs);
    }
    S->gen.shend = nullptr;
    gen_shortid(S, id);
}

static void gen_fn(Session *const S, const Node *const n)
{
    const uint8_t f = n->op;
    S->comp.iload = S->comp.iloadend = nullptr;
    if (!n->nargs) { /* RND alone */
        comp_int(S, 1);
        farg(S, f);
        comp_call(S, K_FN + f);
        return;
    }
    const uint16_t a = n->b;
    gen(S, a);
    S->comp.p = n->end;
    if (f >= TK_MID - TK_SGN - 2) { /* LEFT$, RIGHT$, MID$ */
        comp_tostring(S);
        const uint16_t b = node(S, a)->next;
        gen(S, b);
        comp_toaddr(S);
        uint8_t k = f;
        if (f == TK_MID - TK_SGN) {
            const uint16_t c = node(S, b)->next;
            if (c != NO_NODE) {
                gen(S, c);
                comp_toaddr(S);
            } else
                k = K_MID2 - K_FN;
        }
        S->comp.type = T_STR;
        comp_call(S, K_FN + k);
        return;
    }
    const uint8_t df = fns_dfnk[f];
    if (S->comp.type == T_INT && df == DFN_STR) /* STR$ of an INT: through a DOUBLE, every digit */
        comp_todouble(S);
    else if (n->dbl && !(S->comp.type & (T_STR | T_DBL)) && df < DFN_STR)
        comp_todouble(S); /* for a DOUBLE: of an INT or a SINGLE a DOUBLE's, as BC */
    if (S->comp.type == T_DBL && df != DFN_NONE) {
        if (df == DFN_STR) {
            comp_call(S, K_DSTR);
            S->comp.type = T_STR;
            return;
        }
        comp_opb(S, O_D_FN, df);
        S->comp.type = df == DF_SGN ? T_INT : T_DBL;
        S->comp.iload = S->comp.iloadend = nullptr;
        return;
    }
    farg(S, f);
    if (f == TK_VAL - TK_SGN) { /* VAL: a DOUBLE, as BC's */
        S->comp.type = T_DBL;
        comp_call(S, K_DVAL);
    } else
        comp_call(S, K_FN + f);
}

static void gen_xfn(Session *const S, const Node *const n)
{
    const XFn *const f = &fns_x[n->op];
    if (n->mode == XM_NONE)
        comp_int(S, 0);
    for (uint16_t a = n->b; a != NO_NODE; a = node(S, a)->next) {
        gen(S, a);
        S->comp.p = n->end;
        if (node(S, a)->conv != 'R')
            xarg(S, (char)node(S, a)->conv);
    }
    if (n->mode == XM_COUNT)
        comp_int(S, n->nargs);
    if (f->k)
        comp_call(S, f->k);
    S->comp.type = S->gen.fixdbl ? T_DBL : f->res;
    S->gen.fixdbl = false;
    S->comp.iload = S->comp.iloadend = nullptr;
}

/* A place's address: the subscripts and RA.ADDR id n, R.ADDR id, V.ADDR id. */
static void gen_ref(Session *const S, const Node *const n)
{
    if (n->op == O_RA_ADDR) {
        for (uint16_t a = n->b; a != NO_NODE; a = node(S, a)->next)
            gen_as(S, a, T_INT, false);
        comp_opw(S, O_RA_ADDR, n->id);
        comp_byte(S, n->nargs);
    } else
        comp_opid(S, n->op, n->id);
    S->comp.type = T_INT;
    S->comp.iload = S->comp.iloadend = nullptr;
}

/* A record's field's value: SF.LD off fix, FL.LD off et. */
static void gen_field(Session *const S, const Node *const n)
{
    gen(S, n->a);
    const RecRef *const rec = &node(S, n->a)->rec;
    if (rec->et == ET_SD) {
        comp_opw(S, O_SF_LD, rec->off);
        comp_word(S, rec->fix);
    } else {
        comp_opw(S, O_FL_LD, rec->off);
        comp_byte(S, rec->et);
    }
    S->comp.type = n->type;
    S->comp.iload = S->comp.iloadend = nullptr;
    set_class(S, n->sh);
}

/* A place's address, a record's field's offset added; CALL op after it. */
static void gen_addr(Session *const S, const Node *const n)
{
    gen(S, n->a);
    const Node *const at = node(S, n->a);
    if (at->kind == N_REF && at->rec.off) {
        comp_int(S, at->rec.off);
        comp_op(S, O_I_ADD);
    }
    if (n->op)
        comp_call(S, n->op);
    S->comp.type = T_INT;
    S->comp.iload = S->comp.iloadend = nullptr;
}

/* A call's argument, as its PA_* passes it. */
static void gen_arg(Session *const S, const Node *const n)
{
    switch (n->op) {
    case PA_VALUE:
        gen(S, n->a);
        comp_toinfo(S, symb_var(&S->symb, n->id)->info);
        comp_int(S, 0xFFFF);
        break;
    case PA_VAR:
        comp_int(S, 0);
        gen(S, n->a);
        break;
    case PA_PREF:
        gen(S, n->a);
        comp_opw(S, O_P_REF, node(S, n->a)->rec.off);
        break;
    case PA_ARRAY:
        comp_int(S, n->id);
        comp_int(S, n->id);
        break;
    default: /* PA_RECORD */
        comp_int(S, n->id);
        comp_int(S, 0xFFFF);
    }
}

/* A procedure's call: its arguments, PCALL; one DECLAREd only: ERROR, its result's load. */
static void gen_call(Session *const S, const Node *const n)
{
    if (n->mode == CALL_UNDEF) {
        ctrl_undef(S, QB_NO_FUNCTION);
        loadvar(S, n->id);
        return;
    }
    for (uint16_t a = n->b; a != NO_NODE; a = node(S, a)->next)
        gen(S, a);
    ctrl_pcall(S, n->op);
    S->comp.type = n->type;
    S->comp.iload = S->comp.iloadend = nullptr;
}
