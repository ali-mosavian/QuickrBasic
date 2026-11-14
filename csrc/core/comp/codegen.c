/* The code generator: a tree (comp/ast.h) to stack-machine code. The only module that writes an
   expression's or a reference's bytes. Every operand is pushed; an operator takes the cells it
   needs. A right operand that is one load folds into the operator: F.ADD.V, I.CMP.U8 ... */
#include "comp/codegen.h"
#include "session.h"
#include "comp/control.h"
#include "comp/dfa.h"
#include "comp/builtins.h"
#include "mem/symbols.h"
#include "num/double.h"
#include "num/fpmath.h"
#include "vm/flow.h"

static const uint8_t xop[] = {O_I_DIV, O_I_MOD, O_I_XOR, O_I_EQV, O_I_IMP}; /* from OP_IDIV */
static const uint8_t fcmpk[4] = {O_F_CMP_V, O_F_CMP_VI, O_F_CMP_U8, O_F_CMP_F32};
static const uint8_t lda1[5] = {O_F_LD_A1, O_I_LD_A1, O_S_LD_A1, O_D_LD_A1, O_D_LD_A1};
static const uint8_t ldan[5] = {O_F_LD_A, O_I_LD_A, O_S_LD_A, O_D_LD_A, O_D_LD_A};

static void gen(Code *c, const Node *n);
static void loadvar(Code *c, const Node *n);
static void cg_elem(Code *c, const Node *n);
static void cg_fn(Code *c, const Node *n);
static void cg_xfn(Code *c, const Node *n);
static void cg_ref(Code *c, const Node *n);
static void cg_field(Code *c, const Node *n);
static void cg_addr(Code *c, const Node *n);
static void cg_arg(Code *c, const Node *n);
static void cg_call(Code *c, const Node *n);
static void cg_conv(Code *c, const Node *n);

/* cm_shid: variable v's load just made: a SHORT's, class 1. */
static void shortid(Code *const c, const Var *const v)
{
    if (v->type == ET_SH) {
        c->sh = SH_SHORT;
        c->shend = c->out;
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

static bool isright(Code *const c, const uint8_t op, const uint32_t size, const uint8_t *const right)
{
    return (uint32_t)(c->out - right) == size && *right == op;
}

uint8_t *cg_lfloat(Code *const c, uint8_t *const left, uint8_t *const right)
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
    code_insert(c, O_I_TOF, right);
    return right + 1;
}

void cg_icmp(Code *const c, const uint8_t rel, uint8_t *const right)
{
    c->type = T_INT;
    const uint8_t m = rel & REL_MASK;
    uint8_t op;
    if (isright(c, O_I_LD_V, 3, right))
        op = O_I_CMP_V;
    else if (isright(c, O_I_LD_U8, 2, right))
        op = O_I_CMP_U8;
    else {
        c->cmp = c->out;
        code_opb(c, O_I_CMP, m);
        c->cmpend = c->out;
        return;
    }
    c->cmp = right;
    *right = op;
    code_byte(c, m);
    c->cmpend = c->out;
}

/* The right operand's load that folds: the first op of its group, F_ADD_V .. F_ADD_F32; 0 none. */
static uint8_t fltkind(Code *const c, const uint8_t *const right)
{
    if (isright(c, O_F_LD_V, 3, right))
        return O_F_ADD_V;
    if (isright(c, O_F_LD_VI, 3, right))
        return O_F_ADD_VI;
    if (isright(c, O_F_LD_U8, 2, right))
        return O_F_ADD_U8;
    if (isright(c, O_F_LD_F32, 5, right))
        return O_F_ADD_F32;
    return 0;
}

void cg_fcmp(Code *const c, const uint8_t rel, uint8_t *const right)
{
    c->type = T_INT;
    const uint8_t m = rel & REL_MASK;
    const uint8_t k = fltkind(c, right);
    if (k) {
        c->cmp = right;
        *right = fcmpk[(k - O_F_ADD_V) >> 2];
        code_byte(c, m);
    } else {
        c->cmp = c->out;
        code_opb(c, O_F_CMP, m);
    }
    c->cmpend = c->out;
}

/* A leaf's value as one load of type t (T_INT or T_FLT): its op and operand in *op, b; false:
   not one load. left: a left operand's, which converts only I.LD.V and I.LD.U8 (cm_lflt). */
static bool one_load(Code *const c, const Node *const n, const uint8_t t, const bool left, uint8_t *const op,
                     uint8_t b[static 4])
{
    if (n->kind == N_VAR) {
        const uint8_t info = n->var->info;
        if (info & SK_DBL || n->par)
            return false;
        store16(b, n->id);
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
static void emit_load(Code *const c, const uint8_t op, const uint8_t lop, const uint8_t b[static 4])
{
    code_op(c, op);
    for (uint32_t k = 0; k < operand_size(lop); k++)
        code_byte(c, b[k]);
}

/* Node i's value as type t, left a left operand's. */
static void cg_as(Code *const c, const Node *const i, const uint8_t t, const bool left)
{
    uint8_t op, b[4];
    const Node *const n = i;
    if (t == T_FLT && ast_type(i) == T_INT && one_load(c, n, T_FLT, left, &op, b)) {
        c->shend = nullptr;
        emit_load(c, op, op, b);
        c->type = T_FLT;
        return;
    }
    gen(c, i);
    switch (t) {
    case T_INT: code_toint(c); break;
    case T_DBL: code_todouble(c); break;
    case T_FLT:
        if (left && c->type == T_INT) { /* not the leaf rewritten: I.TOF after it */
            c->type = T_FLT;
            code_op(c, O_I_TOF);
        } else
            code_tofloat(c);
        break;
    default: break;
    }
}

/* Leaf n's SHORT class: a SHORT variable's, a literal's. */
static uint8_t leaf_class(Code *const c, const Node *const n)
{
    if (n->kind == N_VAR)
        return n->var->type == ET_SH ? SH_SHORT : SH_NONE;
    return n->sh;
}

/* A relation's compare op (its right's load lop, operand b, folded in; b null none), mask m
   after it; comp.cmp marks it for a one-line IF. */
static void compare(Code *const c, const uint8_t op, const uint8_t lop, const uint8_t *const b, const uint8_t m)
{
    c->cmp = c->out;
    if (b)
        emit_load(c, op, lop, b);
    else
        code_op(c, op);
    code_byte(c, m);
    c->cmpend = c->out;
    c->type = T_INT;
}

/* INT or FLOAT operator rel (+ - * / AND OR as 0..5, a relation REL | mask) after its left
   operand: the right one as type t, its one load folded into the op (.V, .VI, .U8, .F32). Its
   SHORT class to *rsh. */
static void numeric(Code *const c, const uint8_t rel, const Node *const r, const uint8_t t, uint8_t *const rsh)
{
    const bool flt = t == T_FLT;
    uint8_t lop, b[4];
    if (one_load(c, r, t, false, &lop, b)) {
        *rsh = leaf_class(c, r);
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
                compare(c, op, lop, b, rel & REL_MASK);
            } else {
                const uint8_t base = flt ? (uint8_t[]){O_F_ADD_V, O_F_ADD_VI, O_F_ADD_U8, O_F_ADD_F32}[form]
                                         : form ? O_I_ADD_U8 : O_I_ADD_V;
                emit_load(c, base + rel, lop, b);
                c->type = t;
            }
            return;
        }
    }
    cg_as(c, r, t, false);
    *rsh = code_shclass(c);
    if (ast_isrel(rel))
        compare(c, flt ? O_F_CMP : O_I_CMP, 0, nullptr, rel & REL_MASK);
    else {
        code_op(c, (flt ? O_F_ADD : O_I_ADD) + rel);
        c->type = t;
    }
}

/* Binary node n: its operands as its work type, then the operator. */
static void cg_binary(Code *const c, const Node *const n)
{
    const uint8_t rel = n->op;
    const uint8_t w = ast_work(n);
    uint8_t lsh = SH_NONE, rsh = SH_NONE;
    if (w == W_STR) {
        gen(c, n->a);
        gen(c, n->b);
        if (rel == OP_ADD)
            code_op(c, O_S_CAT);
        else
            compare(c, O_S_CMP, 0, nullptr, rel & REL_MASK);
        c->sh = SH_NONE;
        c->shend = c->out;
        return;
    }
    const uint8_t lt = w == W_INT ? T_INT : w == W_DBL ? T_DBL : T_FLT;
    cg_as(c, n->a, lt, true);
    lsh = code_shclass(c);
    switch (w) {
    case W_DBL:
        cg_as(c, n->b, T_DBL, false);
        rsh = code_shclass(c);
        c->iload = c->iloadend = nullptr;
        if (rel == OP_POW)
            code_op(c, O_D_POW);
        else if (ast_isrel(rel)) {
            code_opb(c, O_D_CMP, rel & REL_MASK);
            c->type = T_INT;
        } else
            code_op(c, O_D_ADD + rel);
        break;
    case W_POW:
        cg_as(c, n->b, T_FLT, false);
        rsh = code_shclass(c);
        code_call(c, K_FPOW);
        c->type = T_FLT;
        break;
    default:
        if (ast_isxop(rel)) {
            cg_as(c, n->b, T_INT, false);
            rsh = code_shclass(c);
            code_op(c, xop[rel - OP_IDIV]);
            c->type = T_INT;
        } else {
            numeric(c, ast_islogic(rel) ? rel - 1 : rel, n->b, lt, &rsh); /* AND, OR: O_I_ADD's 4th, 5th */
        }
    }
    const uint8_t sh = shortop(rel, lsh, rsh);
    if (sh && c->type == T_INT) /* a SHORT's result checked */
        code_op(c, O_I_CHK16);
    c->sh = sh;
    c->shend = c->out;
}

/* Unary node n: - or NOT of its operand. */
static void cg_unary(Code *const c, const Node *const n)
{
    gen(c, n->a);
    if (n->op == TK_NOT) {
        code_toint(c);
        code_op(c, O_I_NOT);
    } else if (c->type == T_DBL)
        code_op(c, O_D_NEG);
    else if (c->type == T_FLT)
        code_op(c, O_F_NEG);
    else {
        code_shclass(c); /* a SHORT's negated is one */
        code_op(c, O_I_NEG);
        c->sh = c->shclass;
        c->shend = c->out;
    }
}

/* The SHORT class of the code just made, as a leaf's. */
static void set_class(Code *const c, const uint8_t sh)
{
    if (sh) {
        c->sh = sh;
        c->shend = c->out;
    }
}

/* Leaf n's code. */
static void cg_leaf(Code *const c, const Node *const n)
{
    switch (n->kind) {
    case N_LIT: {
        uint8_t *const at = c->out;
        const uint8_t b[1] = {n->op};
        const uint32_t size = flow_opsize(b);
        code_op(c, n->op);
        for (uint32_t i = 1; i < size; i++)
            code_byte(c, n->lit[i - 1]);
        c->type = n->type;
        if (n->type == T_INT) {
            c->iload = at;
            c->iloadend = c->out;
        }
        break;
    }
    default: /* N_VAR */
        loadvar(c, n);
        return;
    }
    set_class(c, n->sh);
}

static void gen(Code *const c, const Node *const i)
{
    const Node *const n = i;
    c->shend = nullptr; /* not a SHORT's unless it says */
    switch (n->kind) {
    case N_UN: cg_unary(c, n); return;
    case N_BIN: cg_binary(c, n); return;
    case N_ELEM: cg_elem(c, n); return;
    case N_FN: cg_fn(c, n); break;
    case N_XFN: cg_xfn(c, n); break;
    case N_REF: cg_ref(c, n); break;
    case N_FIELD: cg_field(c, n); break;
    case N_ADDR: cg_addr(c, n); break;
    case N_ARG: cg_arg(c, n); break;
    case N_CALL: cg_call(c, n); break;
    case N_KCALL: code_call(c, n->op); break;
    case N_CONV: cg_conv(c, n); break;
    default: cg_leaf(c, n);
    }
}

void cg_tree(Code *const c, const Node *const n) { gen(c, n); }

/* A scalar's value, n's variable's: X.LD.V. */
static void loadvar(Code *const c, const Node *const n)
{
    const uint16_t id = n->id;
    const uint8_t info = n->var->info;
    if (info & SK_DBL) {
        c->type = T_DBL;
        code_opid(c, O_D_LD_V, id, n->par);
        return;
    }
    const uint8_t k = info & VK_MASK;
    if (k == VK_INT) {
        c->iload = c->out;
        code_opid(c, O_I_LD_V, id, n->par);
        c->iloadend = c->out;
        c->type = T_INT;
        shortid(c, n->var);
    } else if (k == VK_STR) {
        c->type = T_STR;
        code_opid(c, O_S_LD_V, id, n->par);
    } else {
        c->type = T_FLT;
        code_opid(c, O_F_LD_V, id, n->par);
    }
}

/* The value as kind k (an XFn's). */
static void xarg(Code *const c, const char k)
{
    switch (k) {
    case 'D':
        code_todouble(c);
        return;
    case 'G': /* FIX's: a DOUBLE's D.FN 3, else CALL FIX */
        if (c->type == T_DBL) {
            c->fixdbl = true;
            code_opb(c, O_D_FN, DF_FIX);
        } else {
            code_tofloat(c);
            code_call(c, K_FIX);
        }
        return;
    case 'W': /* HEX$, OCT$: a SHORT's 16 bits */
        code_shclass(c);
        code_toint(c);
        if (c->shclass == SH_SHORT) {
            code_int(c, 0xFFFF);
            code_op(c, O_I_AND);
        }
        return;
    case 'I':
        code_toint(c);
        return;
    case 'F':
        code_tofloat(c);
        return;
    case 'S':
        return;
    }
    if (c->type & T_STR) /* X: a string or a number, then 1 or 0 */
        code_int(c, 1);
    else {
        code_toint(c);
        code_int(c, 0);
    }
}

/* Function f's (TK_SGN on) argument converted; comp.type its result. */
static void farg(Code *const c, const uint8_t f)
{
    const uint8_t sig = fns_sig[f];
    switch (sig & FA_MASK) {
    case FA_F: code_tofloat(c); break;
    case FA_A: code_toaddr(c); break;
    case FA_B: code_tobyte(c); break;
    case FA_I: code_toint(c); break;
    }
    const uint8_t res = sig & FR_MASK;
    c->type = res == FR_S ? T_STR : res;
}

static void cg_elem(Code *const c, const Node *const n)
{
    for (const Node *a = n->b; a != nullptr; a = a->next)
        cg_as(c, a, T_INT, false);
    const uint16_t id = n->id;
    const uint8_t k = n->var->info & (SK_DBL | VK_MASK);
    c->type = expr_ktype[k];
    c->iload = c->iloadend = nullptr;
    if (n->nargs == 1)
        code_opid(c, lda1[k], id, n->par);
    else {
        code_opid(c, ldan[k], id, n->par);
        code_byte(c, n->nargs);
    }
    c->shend = nullptr;
    shortid(c, n->var);
}

static void cg_fn(Code *const c, const Node *const n)
{
    const uint8_t f = n->op;
    c->iload = c->iloadend = nullptr;
    if (!n->nargs) { /* RND alone */
        code_int(c, 1);
        farg(c, f);
        code_call(c, K_FN + f);
        return;
    }
    const Node *const a = n->b;
    gen(c, a);
    if (f >= TK_MID - TK_SGN - 2) { /* LEFT$, RIGHT$, MID$ */
        const Node *const b = a->next;
        gen(c, b);
        code_toaddr(c);
        uint8_t k = f;
        if (f == TK_MID - TK_SGN) {
            const Node *const len = b->next;
            if (len) {
                gen(c, len);
                code_toaddr(c);
            } else
                k = K_MID2 - K_FN;
        }
        c->type = T_STR;
        code_call(c, K_FN + k);
        return;
    }
    const uint8_t df = fns_dfnk[f];
    if (c->type == T_INT && df == DFN_STR) /* STR$ of an INT: through a DOUBLE, every digit */
        code_todouble(c);
    else if (n->dbl && !(c->type & (T_STR | T_DBL)) && df < DFN_STR)
        code_todouble(c); /* for a DOUBLE: of an INT or a SINGLE a DOUBLE's, as BC */
    if (c->type == T_DBL && df != DFN_NONE) {
        if (df == DFN_STR) {
            code_call(c, K_DSTR);
            c->type = T_STR;
            return;
        }
        code_opb(c, O_D_FN, df);
        c->type = df == DF_SGN ? T_INT : T_DBL;
        c->iload = c->iloadend = nullptr;
        return;
    }
    farg(c, f);
    if (f == TK_VAL - TK_SGN) { /* VAL: a DOUBLE, as BC's */
        c->type = T_DBL;
        code_call(c, K_DVAL);
    } else
        code_call(c, K_FN + f);
}

static void cg_xfn(Code *const c, const Node *const n)
{
    const XFn *const f = &fns_x[n->op];
    if (n->mode == XM_NONE)
        code_int(c, 0);
    for (const Node *a = n->b; a != nullptr; a = a->next) {
        gen(c, a);
        if (a->conv != 'R')
            xarg(c, (char)a->conv);
    }
    if (n->mode == XM_COUNT)
        code_int(c, n->nargs);
    if (f->k)
        code_call(c, f->k);
    c->type = c->fixdbl ? T_DBL : f->res;
    c->fixdbl = false;
    c->iload = c->iloadend = nullptr;
}

/* A place's address: the subscripts and RA.ADDR id n, R.ADDR id, V.ADDR id. */
static void cg_ref(Code *const c, const Node *const n)
{
    if (n->op == O_RA_ADDR) {
        for (const Node *a = n->b; a != nullptr; a = a->next)
            cg_as(c, a, T_INT, false);
        code_opw(c, O_RA_ADDR, n->id);
        code_byte(c, n->nargs);
    } else
        code_opid(c, n->op, n->id, n->par);
    c->type = T_INT;
    c->iload = c->iloadend = nullptr;
}

/* A record's field's value: SF.LD off fix, FL.LD off et. */
static void cg_field(Code *const c, const Node *const n)
{
    gen(c, n->a);
    const RecRef *const rec = &n->a->rec;
    if (rec->et == ET_SD) {
        code_opw(c, O_SF_LD, rec->off);
        code_word(c, rec->fix);
    } else {
        code_opw(c, O_FL_LD, rec->off);
        code_byte(c, rec->et);
    }
    c->type = n->type;
    c->iload = c->iloadend = nullptr;
    set_class(c, n->sh);
}

/* A place's address, a record's field's offset added; CALL op after it. */
static void cg_addr(Code *const c, const Node *const n)
{
    gen(c, n->a);
    const Node *const at = n->a;
    if (at->kind == N_REF && at->rec.off) {
        code_int(c, at->rec.off);
        code_op(c, O_I_ADD);
    }
    if (n->op)
        code_call(c, n->op);
    c->type = T_INT;
    c->iload = c->iloadend = nullptr;
}

/* A call's argument, as its PA_* passes it. */
static void cg_arg(Code *const c, const Node *const n)
{
    switch (n->op) {
    case PA_VALUE:
        gen(c, n->a);
        code_toinfo(c, n->var->info);
        code_int(c, 0xFFFF);
        break;
    case PA_VAR:
        code_int(c, 0);
        gen(c, n->a);
        break;
    case PA_PREF:
        gen(c, n->a);
        code_opw(c, O_P_REF, n->a->rec.off);
        break;
    case PA_ARRAY:
        code_int(c, n->id);
        code_int(c, n->id);
        break;
    default: /* PA_RECORD */
        code_int(c, n->id);
        code_int(c, 0xFFFF);
    }
}

/* A procedure's call: its arguments, PCALL; one DECLAREd only: ERROR, its result's load. */
static void cg_call(Code *const c, const Node *const n)
{
    if (n->mode == CALL_UNDEF) {
        code_int(c, QB_NO_FUNCTION); /* ctrl_undef's */
        code_call(c, K_ERROR);
        loadvar(c, n);
        return;
    }
    for (const Node *a = n->b; a != nullptr; a = a->next)
        gen(c, a);
    code_opb(c, O_PCALL, n->op); /* its entry's Jump after it */
    code_dword(c, n->jump.line);
    code_dword(c, n->jump.body);
    code_dword(c, n->jump.dest);
    c->type = n->type;
    c->iload = c->iloadend = nullptr;
}

/* Its child converted. */
static void cg_conv(Code *const c, const Node *const n)
{
    gen(c, n->a);
    switch (n->op) {
    case CV_INT: code_toint(c); break;
    case CV_FLT: code_tofloat(c); break;
    case CV_DBL: code_todouble(c); break;
    case CV_STR: break;
    case CV_BYTE: code_tobyte(c); break;
    default: code_toaddr(c); /* CV_ADDR */
    }
}
