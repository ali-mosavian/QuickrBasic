/* The expression compiler (comp/expr.asm). Precedences: OR 46h, AND 50h, NOT 5Ah, > = < 64h,
   + - 78h, * / 7Bh, unary - 7Dh, ^ 7Fh. */
#include "comp/expr.h"
#include "comp/fns.h"
#include "comp/fold.h"
#include "comp/ref.h"
#include "session.h"
#include "comp/blok.h"
#include "comp/ctrl.h"
#include "comp/decl.h"
#include "comp/dfa.h"
#include "io/repl.h"
#include "mem/scop.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "num/dubl.h"
#include "num/fmth.h"
#include "num/sngl.h"
#include "vm/flow.h"

const uint8_t expr_ktype[5] = {T_FLT, T_INT, T_STR, T_DBL, T_DBL};

/* Precedences: the higher binds tighter. */
enum : uint8_t {
    P_IMP = 0x40, P_EQV = 0x42, P_XOR = 0x44, P_OR = 0x46, P_AND = 0x50, P_NOT = 0x5A,
    P_REL = 0x64, P_ADD = 0x78, P_MOD = 0x79, P_IDIV = 0x7A, P_MUL = 0x7B, P_NEG = 0x7D,
    P_POW = 0x7F,
};
static const uint8_t prectab[NOPS] = {
    [OP_ADD] = P_ADD,   [OP_SUB] = P_ADD, [OP_MUL] = P_MUL, [OP_DIV] = P_MUL, [OP_POW] = P_POW,
    [OP_AND] = P_AND,   [OP_OR] = P_OR,   [OP_GT] = P_REL,  [OP_EQ] = P_REL,  [OP_LT] = P_REL,
    [OP_IDIV] = P_IDIV, [OP_MOD] = P_MOD, [OP_XOR] = P_XOR, [OP_EQV] = P_EQV, [OP_IMP] = P_IMP,
};
static const uint8_t relbit[] = {DC_GT, DC_EQ, DC_LT};                      /* from OP_GT */

[[noreturn]] static void synerr(Session *const S) { repl_error(S, E_SN); }
[[noreturn]] static void mismatch(Session *const S) { repl_error(S, E_TM); }

static uint16_t parse(Session *S, uint8_t p);
static uint16_t parse_unary(Session *S);
static uint16_t addr_node(Session *S, uint16_t at, uint8_t k);
static uint16_t var_leaf(Session *S);
static uint16_t str_leaf(Session *S);

/* An expression parsed, folded and compiled, for a DOUBLE target or not; its tree dropped. */
static void compile(Session *const S, const bool dbl)
{
    const uint16_t nodes = S->ast.nnodes;
    const bool was = S->expr.dbltgt;
    S->expr.dbltgt = dbl;
    const uint16_t root = parse(S, 0);
    S->expr.dbltgt = was;
    const uint8_t *const end = S->comp.p;
    fold_tree(S, root);
    gen_tree(S, root);
    S->comp.p = end;
    S->ast.nnodes = nodes;
}

void expr_compile(Session *const S) { compile(S, false); }

void expr_assign(Session *const S) { compile(S, S->comp.tgtinfo & SK_DBL); }

/* The operator at c's index; false: none. */
static bool operator_of(const uint8_t c, uint8_t *const op)
{
    if (c == '\\')
        *op = OP_IDIV;
    else if (c >= TK_MOD && c <= TK_IMP)
        *op = c - TK_MOD + OP_MOD;
    else if (c >= TK_PLUS && c <= TK_LT)
        *op = c - TK_PLUS;
    else
        return false;
    return true;
}

/* A run of > = < from op: REL and their mask, comp.p past it. */
static uint8_t relation(Session *const S, uint8_t op)
{
    uint8_t rel = REL;
    for (;;) {
        rel |= relbit[op - OP_GT];
        const uint8_t c = comp_curn(&S->comp);
        if (c < TK_GT || c > TK_LT)
            return rel;
        op = c - TK_PLUS;
    }
}

static Node *node(Session *const S, const uint16_t i) { return ast_node(S, i); }

/* cm_p: the operand and the operators after it that bind tighter than precedence p. */
static uint16_t parse(Session *const S, const uint8_t p)
{
    if (++S->comp.depth > COMP_MAXDEPTH)
        repl_error(S, E_OM);
    uint16_t left = parse_unary(S);
    uint8_t op;
    while (operator_of(comp_cur(&S->comp), &op) && p < prectab[op]) {
        uint8_t rel = op;
        if (op >= OP_GT && op <= OP_LT)
            rel = relation(S, op);
        else
            S->comp.p++;
        const uint16_t right = parse(S, prectab[op]);
        const uint16_t n = ast_new(S, N_BIN);
        *node(S, n) = (Node){.kind = N_BIN, .op = rel, .dbl = S->expr.dbltgt, .a = left, .b = right,
                             .end = S->comp.p};
        left = n;
    }
    S->comp.depth--;
    return left;
}

void expr_emit(Session *const S, const uint16_t root, const uint16_t mark)
{
    const uint8_t *const end = S->comp.p;
    fold_tree(S, root);
    gen_tree(S, root);
    S->comp.p = end;
    S->ast.nnodes = mark;
}

void expr_paren(Session *const S)
{
    comp_open(S);
    expr_compile(S);
    comp_close(S);
}

uint8_t expr_subs(Session *const S)
{
    comp_open(S);
    uint8_t n = 0;
    do {
        n++;
        expr_compile(S);
        comp_toint(S);
    } while (comp_optcomma(S));
    comp_close(S);
    return n;
}

void expr_string(Session *const S)
{
    const uint16_t mark = S->ast.nnodes;
    expr_emit(S, str_leaf(S), mark);
}

void expr_load(Session *const S)
{
    const uint16_t mark = S->ast.nnodes;
    expr_emit(S, var_leaf(S), mark);
}

void expr_target(Session *const S)
{
    const uint16_t mark = S->ast.nnodes;
    const uint16_t r = comp_cur(&S->comp) == M_VAR ? ref_record(S) : NO_NODE;
    if (r != NO_NODE) { /* a record's field: its address */
        const RecRef rec = node(S, r)->rec;
        if (rec.et >= ET_REC)
            mismatch(S);
        expr_emit(S, r, mark);
        S->comp.tgt = rec.off;
        S->comp.tgtsubs = TGT_FIELD;
        S->ctrl.fieldet = rec.et;
        S->ctrl.fieldfix = rec.fix;
        S->comp.tgtinfo = comp_etkind[rec.et];
        return;
    }
    const uint16_t id = comp_name(S);
    if (!(symb_var(&S->symb, id)->info & VI_ARR)) {
        comp_target(S, id);
        S->comp.vat = S->comp.out;
        return;
    }
    const uint8_t n = expr_subs(S); /* may compile a call with its own target */
    comp_target(S, id);
    S->comp.tgtsubs = n;
    S->comp.vat = S->comp.out;
}


void expr_varaddr(Session *const S)
{
    const uint16_t mark = S->ast.nnodes;
    uint32_t vk;
    expr_emit(S, addr_node(S, ref_place(S, &vk), 0), mark);
    comp_int(S, vk);
}

/* The function token at comp.p (FFh n as 100h + n), comp.p past it: its XFn. */
static const XFn *xfn_at(Session *const S)
{
    uint16_t tok = *S->comp.p;
    if (tok == TK_X)
        tok = 0x100 + *++S->comp.p;
    S->comp.p++;
    for (size_t i = 0; i < fns_nx; i++)
        if (fns_x[i].tok == tok)
            return &fns_x[i];
    synerr(S);
}

/* VARPTR, VARSEG, SSEG, SADD (v) after FFh: its CALL in k, 0 none; false: not one. */
static bool addr_call(const uint8_t tok, uint8_t *const k)
{
    switch (tok) {
    case TK1_VARPTR: *k = 0; return true;
    case TK1_VARSEG:
    case TK1_SSEG: *k = K_VSEG; return true;
    case TK1_SADD: *k = K_SADD; return true;
    default: return false;
    }
}

static uint16_t lit_leaf(Session *S, uint8_t op, const uint8_t *bytes, uint8_t sh);

uint16_t expr_arg(Session *const S)
{
    const bool was = S->expr.dbltgt;
    S->expr.dbltgt = false;
    const uint16_t n = parse(S, 0);
    S->expr.dbltgt = was;
    return n;
}

static uint16_t args_node(Session *const S, const NodeKind kind)
{
    const uint16_t n = ast_new(S, kind);
    node(S, n)->b = NO_NODE;
    return n;
}

/* Array id's element: ( e {, e} ). */
static uint16_t elem_node(Session *const S, const uint16_t id)
{
    const uint16_t n = args_node(S, N_ELEM);
    node(S, n)->id = id;
    node(S, n)->type = expr_ktype[symb_var(&S->symb, id)->info & (SK_DBL | VK_MASK)];
    expr_args(S, n);
    return n;
}

void expr_args(Session *const S, const uint16_t n)
{
    uint16_t last = NO_NODE;
    comp_open(S);
    do
        ast_link(S, n, &last, expr_arg(S));
    while (comp_optcomma(S));
    comp_close(S);
}

/* A string literal after its quote: S.LD.TXT len, its text's Addr. */
static uint16_t str_leaf(Session *const S)
{
    const uint8_t *const start = S->comp.p;
    const uint8_t *q = start;
    uint8_t b[5];
    b[0] = (uint8_t)strg_span(&q, '"', '"');
    store32(b + 1, addr_of(start));
    S->comp.p = strg_skipq(q);
    return lit_leaf(S, O_S_LD_TXT, b, SH_NONE);
}

/* SGN .. MID$ (f, TK_SGN on): (e), LEFT$ RIGHT$ (s, n), MID$ (s, n [, n]); RND alone. */
static uint16_t fn_node(Session *const S, const uint8_t f)
{
    const uint16_t n = args_node(S, N_FN);
    node(S, n)->op = f;
    node(S, n)->dbl = S->expr.dbltgt;
    S->comp.p++;
    if (f == TK_RND - TK_SGN && *S->comp.p != '(') { /* RND alone: RND(1) */
        node(S, n)->type = T_FLT;
        return n;
    }
    uint16_t last = NO_NODE;
    comp_open(S);
    ast_link(S, n, &last, expr_arg(S));
    if (f >= TK_MID - TK_SGN - 2) {
        comp_comma(S);
        ast_link(S, n, &last, expr_arg(S));
        if (f == TK_MID - TK_SGN && comp_optcomma(S))
            ast_link(S, n, &last, expr_arg(S));
    }
    comp_close(S);
    node(S, n)->type = fns_type(f, ast_type(S, node(S, n)->b), S->expr.dbltgt);
    node(S, n)->end = S->comp.p;
    return n;
}

/* QBasic's other functions, by fns_x: f(args) each of its kind, their count when it varies. */
static uint16_t xfn_node(Session *const S)
{
    const XFn *const f = xfn_at(S);
    const uint16_t n = args_node(S, N_XFN);
    node(S, n)->op = (uint8_t)(f - fns_x);
    node(S, n)->type = f->res;
    if (!f->max)
        return n;
    if (!(f->min & XF_MIN) && comp_cur(&S->comp) != '(') { /* all optional, ( ) left out */
        node(S, n)->mode = XM_NONE;
        return n;
    }
    uint16_t last = NO_NODE;
    comp_open(S);
    for (uint32_t i = 0;; i++) {
        char k = f->arg[i];
        uint16_t a;
        if (k == 'R') { /* an array, n or n(): its id */
            if (comp_cur(&S->comp) != M_VAR)
                synerr(S);
            const uint16_t id = comp_var(S, 2, 0);
            if (*S->comp.p == '(') {
                if (S->comp.p[1] != ')')
                    synerr(S);
                S->comp.p += 2;
            }
            a = expr_intleaf(S, id, SH_NONE);
        } else {
            if (k == 'H') { /* a file number, its # or not */
                if (comp_cur(&S->comp) == '#')
                    S->comp.p++;
                k = 'I';
            }
            a = expr_arg(S);
            if (node(S, n)->nargs == 0 && f->min & XF_LEAD && ast_type(S, a) == T_STR && k != 'S')
                k = f->arg[++i]; /* a leading optional left out */
            if (k == 'G' && ast_type(S, a) == T_DBL) /* FIX of a DOUBLE: D.FN's */
                node(S, n)->type = T_DBL;
        }
        node(S, a)->conv = (uint8_t)k;
        ast_link(S, n, &last, a);
        if (i + 1 >= f->max || !comp_optcomma(S)) {
            if (i + 1 < (f->min & XF_MIN))
                synerr(S);
            break;
        }
    }
    comp_close(S);
    node(S, n)->mode = (f->min & XF_MIN) != f->max ? XM_COUNT : XM_ARGS;
    node(S, n)->end = S->comp.p;
    return n;
}



/* VARPTR, VARSEG, SSEG, SADD of place at: CALL k after it, 0 none; cm_va's when k is 0. */
static uint16_t addr_node(Session *const S, const uint16_t at, const uint8_t k)
{
    const uint16_t n = ast_new(S, N_ADDR);
    node(S, n)->a = at;
    node(S, n)->op = k;
    node(S, n)->type = T_INT;
    return n;
}

/* A literal load op and its operand bytes as a leaf. */
static uint16_t lit_leaf(Session *const S, const uint8_t op, const uint8_t *const bytes, const uint8_t sh)
{
    const uint16_t i = ast_new(S, N_LIT);
    Node *const n = node(S, i);
    n->op = op;
    n->type = fold_littype(op);
    n->sh = sh;
    const uint8_t b[1] = {op};
    const uint32_t size = flow_opsize(b);
    for (uint32_t k = 1; k < size; k++)
        n->lit[k - 1] = bytes[k - 1];
    return i;
}

/* An INT literal as I.LD.U8, I.LD.I16 or I.LD.I32. */
uint16_t expr_intleaf(Session *const S, const uint32_t v, const uint8_t sh)
{
    uint8_t b[4];
    store32(b, v);
    const uint8_t op = v > 0xFFFF ? O_I_LD_I32 : v > 0xFF ? O_I_LD_I16 : O_I_LD_U8;
    return lit_leaf(S, op, b, sh);
}

/* A variable's value at comp.p: a FUNCTION's call, a record's field, an element, a CONST's
   literal, a scalar. */
static uint16_t var_leaf(Session *const S)
{
    const Name *const nm = symb_name(&S->symb, comp_nameof(S->comp.p));
    if (nm->flags & (NM_FUNC | NM_FN)) { /* in FUNCTION f, f alone is its result */
        if (!nm->proc)
            return ref_undef(S);
        if (*comp_skipname(S->comp.p) == '(' || nm->proc != S->comp.proc) {
            uint8_t idx;
            const Proc *const pr = ref_proc(S, &idx);
            return ref_call(S, idx, pr, ARGS_PAREN);
        }
    }
    const uint16_t r = ref_record(S);
    if (r != NO_NODE)
        return ref_field(S, r);
    const uint16_t id = comp_name(S);
    if (symb_var(&S->symb, id)->info & VI_ARR)
        return elem_node(S, id);
    const uint8_t kop = symb_varx(&S->symb, id)->kop;
    if (kop)
        return lit_leaf(S, kop, symb_kval(&S->symb, id),
                        symb_var(&S->symb, id)->type == ET_SH ? SH_SHORT : SH_NONE);
    const uint16_t i = ast_new(S, N_VAR);
    node(S, i)->id = id;
    return i;
}

/* A number's literal: a DOUBLE's D.LD.K, else F.LD.F32. */
static uint16_t num_leaf(Session *const S)
{
    uint8_t kind;
    const double d = dubl_in(S, &S->comp.p, &kind);
    uint8_t b[8];
    if (kind == DK_DBL) {
        const uint64_t q = (Cell){.d = d}.raw;
        store32(b, (uint32_t)q);
        store32(b + 4, (uint32_t)(q >> 32));
        return lit_leaf(S, O_D_LD_K, b, SH_NONE);
    }
    store32(b, sngl_from_dbl(d));
    return lit_leaf(S, O_F_LD_F32, b, SH_NONE);
}

/* cm_un: an operand: a variable, an element, a literal, ( expr ), - NOT +, a function. */
static uint16_t parse_unary(Session *const S)
{
    for (;;) {
        const uint8_t c = comp_cur(&S->comp);
        switch (c) {
        case M_VAR:
            return var_leaf(S);
        case M_INT: {
            const uint32_t v = load32(S->comp.p + 1);
            S->comp.p += INT_TOKEN_SIZE;
            return expr_intleaf(S, v, v <= INT16_MAX ? SH_LIT : SH_NONE);
        }
        case '(': { /* its own: not for a DOUBLE target */
            comp_open(S);
            const bool was = S->expr.dbltgt;
            S->expr.dbltgt = false;
            const uint16_t n = parse(S, 0);
            S->expr.dbltgt = was;
            comp_close(S);
            return n;
        }
        case TK_MINUS:
        case TK_NOT: {
            S->comp.p++;
            const uint16_t a = parse(S, c == TK_MINUS ? P_NEG : P_NOT);
            const uint16_t n = ast_new(S, N_UN);
            node(S, n)->op = c;
            node(S, n)->a = a;
            node(S, n)->end = S->comp.p;
            return n;
        }
        case TK_PLUS:
            S->comp.p++;
            continue;
        case '"':
            S->comp.p++;
            return str_leaf(S);
        }
        if (c >= '.' && c <= '9')
            return num_leaf(S);
        uint32_t size;
        if (c == TK_LEN && ref_len(S, &size))
            return expr_intleaf(S, size, SH_LIT);
        uint8_t k;
        if (c == TK_X && addr_call(S->comp.p[1], &k)) {
            S->comp.p += 2;
            comp_open(S);
            uint32_t vk;
            const uint16_t at = ref_place(S, &vk);
            comp_close(S);
            return k || !ast_islit(S, at) ? addr_node(S, at, k) : at;
        }
        const uint8_t f = c - TK_SGN;
        return f > TK_MID - TK_SGN ? xfn_node(S) : fn_node(S, f);
    }
}
