/* The expression compiler (comp/expr.asm). Precedences: OR 46h, AND 50h, NOT 5Ah, > = < 64h,
   + - 78h, * / 7Bh, unary - 7Dh, ^ 7Fh. */
#include "comp/expr.h"
#include "comp/fold.h"
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
#include "num/sngl.h"
#include "vm/flow.h"

const uint8_t expr_ktype[5] = {T_FLT, T_INT, T_STR, T_DBL, T_DBL};

/* An operator's index: + - * / ^ AND OR > = < from TK_PLUS, then \ MOD XOR EQV IMP. A relation
   compiles as REL and its DC_* mask. */
enum : uint8_t {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_POW, OP_AND, OP_OR, OP_GT, OP_EQ, OP_LT,
    OP_IDIV, OP_MOD, OP_XOR, OP_EQV, OP_IMP, NOPS,
};
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
static const uint8_t xop[] = {O_I_DIV, O_I_MOD, O_I_XOR, O_I_EQV, O_I_IMP}; /* from OP_IDIV */
static const uint8_t relbit[] = {DC_GT, DC_EQ, DC_LT};                      /* from OP_GT */
static const uint8_t fcmpk[4] = {O_F_CMP_V, O_F_CMP_VI, O_F_CMP_U8, O_F_CMP_F32};
static const uint8_t lda1[5] = {O_F_LD_A1, O_I_LD_A1, O_S_LD_A1, O_D_LD_A1, O_D_LD_A1};
static const uint8_t ldan[5] = {O_F_LD_A, O_I_LD_A, O_S_LD_A, O_D_LD_A, O_D_LD_A};
/* SGN .. CHR$ of a DOUBLE: D.FN's operand, DFN_STR STR$'s, DFN_NONE none. */
enum : uint8_t { DFN_STR = 0x80, DFN_NONE = 0xFF };
#define FN(tk) [TK_##tk - TK_SGN]
static const uint8_t dfnk[20] = {
    FN(SGN) = DF_SGN,    FN(INT) = DF_INT,    FN(ABS) = DF_ABS,    FN(USR) = DFN_NONE,
    FN(FRE) = DFN_NONE,  FN(POS) = DFN_NONE,  FN(SQR) = DF_SQR,    FN(RND) = DFN_NONE,
    FN(LOG) = DF_LOG,    FN(EXP) = DF_EXP,    FN(COS) = DF_COS,    FN(SIN) = DF_SIN,
    FN(TAN) = DF_TAN,    FN(ATN) = DF_ATN,    FN(PEEK) = DFN_NONE, FN(LEN) = DFN_NONE,
    FN(STRS) = DFN_STR,  FN(VAL) = DFN_NONE,  FN(ASC) = DFN_NONE,  FN(CHRS) = DFN_NONE,
};

/* Per function, TK_SGN on: its argument's conversion (FA_*), its result (FR_*). */
enum { FA_F, FA_A, FA_S, FA_Y, FA_B, FA_N, FA_I, FA_MASK = 0x0F };
enum { FR_I = T_INT, FR_F = T_FLT, FR_S = 0xB0, FR_MASK = 0xF0 };
static const uint8_t fnsig[20] = {
    FN(SGN) = FA_F + FR_I,  FN(INT) = FA_F + FR_F,  FN(ABS) = FA_F + FR_F,  FN(USR) = FA_F + FR_F,
    FN(FRE) = FA_Y + FR_F,  FN(POS) = FA_N + FR_I,  FN(SQR) = FA_F + FR_F,  FN(RND) = FA_F + FR_F,
    FN(LOG) = FA_F + FR_F,  FN(EXP) = FA_F + FR_F,  FN(COS) = FA_F + FR_F,  FN(SIN) = FA_F + FR_F,
    FN(TAN) = FA_F + FR_F,  FN(ATN) = FA_F + FR_F,  FN(PEEK) = FA_I + FR_I, FN(LEN) = FA_S + FR_I,
    FN(STRS) = FA_F + FR_S, FN(VAL) = FA_S + FR_F,  FN(ASC) = FA_S + FR_I,  FN(CHRS) = FA_B + FR_S,
};
#undef FN

/* QBasic's other functions: the token (100h + n for TK_X n), the CALL (0 none), the arguments at
   least (| XF_LEAD: a leading one optional), at most (0: no ( ); least 0: ( ) optional), the
   result, each argument's kind: I an INT, W a SHORT's 16 bits, F a FLOAT, D a DOUBLE, S a string,
   X a string or a number, G FIX's, H a file number, R an array. */
typedef struct {
    uint16_t tok;
    uint8_t k, min, max, res;
    char arg[3];
} XFn;
enum : uint8_t { XF_LEAD = 0x80, XF_MIN = 0x7F };
static const XFn xfns[] = {
    {0x100 + TK1_CINT, 0, 1, 1, T_INT, "I"},
    {0x100 + TK1_CLNG, 0, 1, 1, T_INT, "I"},
    {0x100 + TK1_CSNG, 0, 1, 1, T_FLT, "F"},
    {0x100 + TK1_CDBL, 0, 1, 1, T_DBL, "D"},
    {TK_FIX, 0, 1, 1, T_FLT, "G"},
    {TK_INSTR, K_INSTR, XF_LEAD | 2, 3, T_INT, {'I', 'S', 'S'}},
    {TK_LBOUND, K_LBND, 1, 2, T_INT, "RI"},
    {TK_UBOUND, K_UBND, 1, 2, T_INT, "RI"},
    {0x100 + TK1_SPACES, K_SPACE, 1, 1, T_STR, "I"},
    {0x100 + TK1_STRINGS, K_STRG, 2, 2, T_STR, "IX"},
    {0x100 + TK1_LTRIMS, K_LTRIM, 1, 1, T_STR, "S"},
    {0x100 + TK1_RTRIMS, K_RTRIM, 1, 1, T_STR, "S"},
    {0x100 + TK1_UCASES, K_UCASE, 1, 1, T_STR, "S"},
    {0x100 + TK1_LCASES, K_LCASE, 1, 1, T_STR, "S"},
    {0x100 + TK1_HEXS, K_HEX, 1, 1, T_STR, "W"},
    {0x100 + TK1_OCTS, K_OCT, 1, 1, T_STR, "W"},
    {0x100 + TK1_ERR, K_ERR, 0, 0, T_INT, ""},
    {0x100 + TK1_ERL, K_ERL, 0, 0, T_INT, ""},
    {0x100 + TK1_TIMER, K_TIMER, 0, 0, T_FLT, ""},
    {0x100 + TK1_DATES, K_DATE, 0, 0, T_STR, ""},
    {0x100 + TK1_TIMES, K_TIME, 0, 0, T_STR, ""},
    {TK_INKEYS, K_INKEY, 0, 0, T_STR, ""},
    {0x100 + TK1_COMMANDS, K_CMD, 0, 0, T_STR, ""},
    {0x100 + TK1_ENVIRONS, K_ENVS, 1, 1, T_STR, "X"},
    {0x100 + TK1_SETMEM, K_SETMEM, 1, 1, T_INT, "I"},
    {0x100 + TK1_ERDEV, K_ERDEV, 0, 0, T_INT, ""},
    {0x100 + TK1_ERDEVS, K_ERDEVS, 0, 0, T_STR, ""},
    {0x100 + TK1_MKIS, K_MKI, 1, 1, T_STR, "I"},
    {0x100 + TK1_MKLS, K_MKL, 1, 1, T_STR, "I"},
    {0x100 + TK1_MKSS, K_MKS, 1, 1, T_STR, "F"},
    {0x100 + TK1_MKDS, K_MKD, 1, 1, T_STR, "D"},
    {0x100 + TK1_MKSMBFS, K_MKSM, 1, 1, T_STR, "F"},
    {0x100 + TK1_MKDMBFS, K_MKDM, 1, 1, T_STR, "D"},
    {0x100 + TK1_CVI, K_CVI, 1, 1, T_INT, "S"},
    {0x100 + TK1_CVL, K_CVL, 1, 1, T_INT, "S"},
    {0x100 + TK1_CVS, K_CVS, 1, 1, T_FLT, "S"},
    {0x100 + TK1_CVD, K_CVD, 1, 1, T_DBL, "S"},
    {0x100 + TK1_CVSMBF, K_CVSM, 1, 1, T_FLT, "S"},
    {0x100 + TK1_CVDMBF, K_CVDM, 1, 1, T_DBL, "S"},
    {0x100 + TK1_EOF, K_EOF, 1, 1, T_INT, "H"},
    {0x100 + TK1_LOF, K_LOF, 1, 1, T_INT, "H"},
    {0x100 + TK1_LOC, K_LOC, 1, 1, T_INT, "H"},
    {0x100 + TK1_SEEK, K_SEEKF, 1, 1, T_INT, "H"},
    {0x100 + TK1_FREEFILE, K_FREEF, 0, 0, T_INT, ""},
    {0x100 + TK1_INP, K_INP, 1, 1, T_INT, "I"},
    {0x100 + TK1_INPUTS, K_INPS, 1, 2, T_STR, "IH"},
    {0x100 + TK1_FILEATTR, K_FATTR, 2, 2, T_INT, "HI"},
};

[[noreturn]] static void synerr(Session *const S) { repl_error(S, E_SN); }
[[noreturn]] static void mismatch(Session *const S) { repl_error(S, E_TM); }

static uint16_t parse(Session *S, uint8_t p);
static uint16_t parse_unary(Session *S);
static void loadvar(Session *S, uint16_t id);
static void gen(Session *S, uint16_t n);

/* An expression parsed, folded and compiled, for a DOUBLE target or not; its tree dropped. */
static void compile(Session *const S, const bool dbl)
{
    const uint16_t nodes = S->ast.nnodes, code = S->ast.ncode;
    const bool was = S->expr.dbltgt;
    S->expr.dbltgt = dbl;
    const uint16_t root = parse(S, 0);
    S->expr.dbltgt = was;
    const uint8_t *const end = S->comp.p;
    fold_tree(S, root);
    gen(S, root);
    S->comp.p = end;
    S->ast.nnodes = nodes;
    S->ast.ncode = code;
}

void expr_compile(Session *const S) { compile(S, false); }

void expr_assign(Session *const S) { compile(S, S->comp.tgtinfo & SK_DBL); }

uint8_t expr_shclass(Session *const S)
{
    S->expr.shclass = S->comp.out == S->expr.shend ? S->expr.sh : SH_NONE;
    return S->expr.shclass;
}

void expr_shortid(Session *const S, const uint16_t id)
{
    if (symb_var(&S->symb, id)->type == ET_SH) {
        S->expr.sh = SH_SHORT;
        S->expr.shend = S->comp.out;
    }
}

static bool is_rel(const uint8_t rel) { return rel & REL; }
static bool is_logic(const uint8_t rel) { return rel == OP_AND || rel == OP_OR; }
static bool is_xop(const uint8_t rel) { return rel >= OP_IDIV && rel < REL; } /* \ MOD XOR EQV IMP */

/* cm_sop: operator rel's result is a SHORT's when both operands are SHORTs or literals that fit,
   one a SHORT: + - * \ MOD, QBasic's INTEGER arithmetic. lsh: the left's class. */
static uint8_t shortop(Session *const S, const uint8_t rel, const uint8_t lsh)
{
    const uint8_t rsh = expr_shclass(S);
    if (!lsh || !rsh || !((lsh | rsh) & SH_SHORT))
        return SH_NONE;
    const bool arith = rel <= OP_MUL || rel == OP_IDIV || rel == OP_MOD;
    return arith ? SH_SHORT : SH_NONE;
}

static bool isright(Session *const S, const uint8_t op, const uint32_t size, const uint8_t *const right)
{
    return (uint32_t)(S->comp.out - right) == size && *right == op;
}

uint8_t *expr_lfloat(Session *const S, uint8_t *const left, uint8_t *const right)
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

/* INT operator o (0..5: + - * / AND OR): the right operand's load folds into .V or .U8. */
static void intop(Session *const S, const uint8_t o, uint8_t *const right)
{
    S->comp.type = T_INT;
    if (isright(S, O_I_LD_V, 3, right))
        *right = O_I_ADD_V + o;
    else if (isright(S, O_I_LD_U8, 2, right))
        *right = O_I_ADD_U8 + o;
    else
        comp_op(S, O_I_ADD + o);
}

void expr_icmp(Session *const S, const uint8_t rel, uint8_t *const right)
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

/* FLOAT operator o (0..3: + - * /): the right's load folded into .V, .VI, .U8 or .F32. */
static void fltop(Session *const S, const uint8_t o, uint8_t *const right)
{
    S->comp.type = T_FLT;
    const uint8_t k = fltkind(S, right);
    if (k)
        *right = k + o;
    else
        comp_op(S, O_F_ADD + o);
}

void expr_fcmp(Session *const S, const uint8_t rel, uint8_t *const right)
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

/* String operator rel: + joins, a relation compares. */
static void strop(Session *const S, const uint8_t rel)
{
    if (!(S->comp.type & T_STR))
        mismatch(S);
    if (rel == OP_ADD)
        comp_op(S, O_S_CAT);
    else if (!is_rel(rel))
        mismatch(S);
    else {
        S->comp.cmp = S->comp.out;
        comp_opb(S, O_S_CMP, rel & REL_MASK);
        S->comp.cmpend = S->comp.out;
        S->comp.type = T_INT;
    }
}

/* A DOUBLE either side; or for a DOUBLE target ^, and unless of constants / or a SINGLE's. */
static bool wants_double(Session *const S, const Node *const n, const uint8_t ltype, const bool bothk)
{
    if (ltype == T_DBL || S->comp.type == T_DBL)
        return true;
    if (!n->dbl)
        return false;
    return n->op == OP_POW || (!bothk && (n->op == OP_DIV || ltype & T_FLT || S->comp.type == T_FLT));
}

/* Number operator rel in DOUBLEs, the left converted before right. */
static void dblop(Session *const S, const uint8_t rel, const uint8_t ltype, uint8_t *const right)
{
    if (ltype != T_DBL)
        comp_insert(S, ltype == T_INT ? O_I_TOD : O_F_TOD, right);
    comp_todouble(S);
    S->comp.iload = S->comp.iloadend = nullptr;
    if (rel == OP_POW)
        comp_op(S, O_D_POW);
    else if (is_rel(rel)) {
        comp_opb(S, O_D_CMP, rel & REL_MASK);
        S->comp.type = T_INT;
    } else
        comp_op(S, O_D_ADD + rel);
}

/* Number operator n->op: DOUBLEs, ^ a SINGLE's, INTs (but /, a FLOAT as QBasic's), or FLOATs. */
static void numop(Session *const S, const Node *const n, const uint8_t ltype, const bool bothk, uint8_t *const left,
                  uint8_t *right)
{
    const uint8_t rel = n->op;
    if (S->comp.type & T_STR)
        mismatch(S);
    const bool lint = !(ltype & T_FLT);
    if (wants_double(S, n, ltype, bothk))
        dblop(S, rel, ltype, right);
    else if (rel == OP_POW) {
        if (lint)
            expr_lfloat(S, left, right);
        comp_tofloat(S);
        comp_call(S, K_FPOW);
        S->comp.type = T_FLT;
    } else if (lint && S->comp.type == T_INT && rel != OP_DIV) {
        if (is_rel(rel))
            expr_icmp(S, rel, right);
        else
            intop(S, rel, right);
    } else {
        if (lint)
            right = expr_lfloat(S, left, right);
        comp_tofloat(S);
        if (is_rel(rel))
            expr_fcmp(S, rel, right);
        else
            fltop(S, rel, right);
    }
}

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

static uint16_t node_new(Session *const S, const NodeKind kind)
{
    if (S->ast.nnodes >= AST_NODES)
        repl_error(S, E_OM);
    const uint16_t i = S->ast.nnodes++;
    S->ast.node[i] = (Node){.kind = kind};
    return i;
}

static Node *node(Session *const S, const uint16_t i) { return &S->ast.node[i]; }

static bool is_lit(Session *const S, const uint16_t i) { return node(S, i)->kind == N_LIT; }

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
        const uint16_t n = node_new(S, N_BIN);
        *node(S, n) = (Node){.kind = N_BIN, .op = rel, .dbl = S->expr.dbltgt, .a = left, .b = right,
                             .end = S->comp.p};
        left = n;
    }
    S->comp.depth--;
    return left;
}

/* Binary node n: its left operand, its right, then the operator; a right load folds into it. */
static void gen_binary(Session *const S, const Node *const n)
{
    const uint8_t rel = n->op;
    gen(S, n->a);
    if (is_logic(rel) || is_xop(rel))
        comp_toint(S);
    const uint8_t ltype = S->comp.type;
    uint8_t *const left = comp_intleaf(S);
    const uint8_t lsh = expr_shclass(S);
    uint8_t *const right = S->comp.out;
    gen(S, n->b);
    S->comp.p = n->end;
    const uint8_t sh = shortop(S, rel, lsh);
    const bool bothk = rel != OP_POW && is_lit(S, n->a) && is_lit(S, n->b);
    if (is_xop(rel)) {
        comp_toint(S);
        comp_op(S, xop[rel - OP_IDIV]);
    } else if (is_logic(rel)) {
        comp_toint(S);
        intop(S, rel - 1, right); /* AND, OR: O_I_ADD's 4th, 5th on */
    } else if (ltype == T_STR)
        strop(S, rel);
    else
        numop(S, n, ltype, bothk, left, right);
    if (sh && S->comp.type == T_INT) /* a SHORT's result checked */
        comp_op(S, O_I_CHK16);
    S->expr.sh = sh;
    S->expr.shend = S->comp.out;
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
        expr_shclass(S); /* a SHORT's negated is one */
        comp_op(S, O_I_NEG);
        S->expr.sh = S->expr.shclass;
        S->expr.shend = S->comp.out;
    }
}

/* The SHORT class of the code just made, as a leaf's. */
static void set_class(Session *const S, const uint8_t sh)
{
    if (sh) {
        S->expr.sh = sh;
        S->expr.shend = S->comp.out;
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
    case N_VAR:
        loadvar(S, n->a);
        return;
    default: /* N_CODE */
        for (uint32_t i = 0; i < n->len; i++)
            comp_byte(S, S->ast.code[n->at + i]);
        S->comp.type = n->type;
        S->comp.iload = S->comp.iloadend = nullptr;
        break;
    }
    set_class(S, n->sh);
}

static void gen(Session *const S, const uint16_t i)
{
    const Node *const n = node(S, i);
    S->expr.shend = nullptr; /* not a SHORT's unless it says */
    switch (n->kind) {
    case N_UN: gen_unary(S, n); return;
    case N_BIN: gen_binary(S, n); return;
    default: gen_leaf(S, n);
    }
}

void expr_gen(Session *const S, const uint16_t n) { gen(S, n); }

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
    S->comp.type = T_STR;
    const uint8_t *const start = S->comp.p;
    const uint8_t *q = start;
    const uint32_t len = strg_span(&q, '"', '"');
    comp_opb(S, O_S_LD_TXT, (uint8_t)len);
    comp_dword(S, addr_of(start));
    S->comp.p = strg_skipq(q);
}

/* A FUNCTION DECLAREd only: its arguments, Function not defined when run; its result's id. */
static uint16_t undeclared(Session *const S)
{
    const uint16_t id = comp_var(S, 1, 0);
    if (*S->comp.p == '(') {
        S->comp.p++;
        ctrl_dropargs(S, true);
        comp_close(S);
    }
    ctrl_undef(S, QB_NO_FUNCTION);
    return id;
}

/* A variable's value, its id's: X.LD.V, or the subscripts and X.LD.A1 id, X.LD.A id n. */
static void loadvar(Session *const S, const uint16_t id)
{
    const uint8_t info = symb_var(&S->symb, id)->info;
    if (info & VI_ARR) {
        const uint8_t n = expr_subs(S);
        const uint8_t k = info & (SK_DBL | VK_MASK);
        S->comp.type = expr_ktype[k];
        if (n == 1)
            comp_opid(S, lda1[k], id);
        else {
            comp_opid(S, ldan[k], id);
            comp_byte(S, n);
        }
        expr_shortid(S, id);
        return;
    }
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
        expr_shortid(S, id);
    } else if (k == VK_STR) {
        S->comp.type = T_STR;
        comp_opid(S, O_S_LD_V, id);
    } else {
        S->comp.type = T_FLT;
        comp_opid(S, O_F_LD_V, id);
    }
}

/* A variable's value at comp.p: a scalar's id, nothing compiled; else NO_ID, its code compiled
   (a FUNCTION's call, a record's field, an element). */
enum : uint16_t { NO_ID = 0xFFFF };
static uint16_t load(Session *const S)
{
    if (comp_cur(&S->comp) == M_VAR) { /* a FUNCTION's name: its call, but its result in it */
        const Name *const n = symb_name(&S->symb, comp_nameof(S->comp.p));
        if (n->flags & (NM_FUNC | NM_FN)) {
            if (!n->proc)
                return undeclared(S);
            if (*comp_skipname(S->comp.p) == '(' || n->proc != S->comp.proc) {
                ctrl_fcall(S);
                return NO_ID;
            }
        }
    }
    if (comp_cur(&S->comp) == M_VAR && ctrl_recref(S)) {
        ctrl_recload(S);
        return NO_ID;
    }
    const uint16_t id = comp_name(S);
    if (!(symb_var(&S->symb, id)->info & VI_ARR))
        return id;
    loadvar(S, id);
    return NO_ID;
}

void expr_load(Session *const S)
{
    const uint16_t id = load(S);
    if (id != NO_ID)
        loadvar(S, id);
}

void expr_target(Session *const S)
{
    if (comp_cur(&S->comp) == M_VAR && ctrl_recref(S)) { /* a record's field: its address */
        S->comp.tgt = S->ctrl.ref.off;
        S->comp.tgtsubs = TGT_FIELD;
        S->ctrl.fieldet = S->ctrl.ref.et;
        if (S->ctrl.ref.et >= ET_REC)
            mismatch(S);
        S->ctrl.fieldfix = S->ctrl.ref.fix;
        S->comp.tgtinfo = comp_etkind[S->ctrl.ref.et];
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

/* ctrl.ref's field: its bytes. */
static uint32_t field_size(Session *const S)
{
    const uint8_t et = S->ctrl.ref.et;
    return et >= ET_REC || et == ET_SD ? S->ctrl.ref.fix : decl_etsize[et];
}

/* cm_addr: a variable's address: a record's or a field's code, else its slot's, its Desc's. */
static void varaddr(Session *const S)
{
    if (comp_cur(&S->comp) != M_VAR)
        synerr(S);
    if (ctrl_recref(S)) {
        S->expr.vakind = va_pack(S->ctrl.ref.et == ET_I2 ? VA_I2 : VA_RAW, field_size(S));
        if (S->ctrl.ref.off) {
            comp_int(S, S->ctrl.ref.off);
            comp_op(S, O_I_ADD);
        }
        return;
    }
    const uint16_t id = comp_var(S, 0, 0);
    S->expr.vakind = vakind(S, id);
    if (symb_var(&S->symb, id)->info & VI_ARR) { /* an element: RA.ADDR's */
        const uint8_t n = expr_subs(S);
        comp_opw(S, O_RA_ADDR, id);
        comp_byte(S, n);
    } else if (comp_ispar(S, id)) /* a parameter's: its cell, the argument's */
        comp_opid(S, O_V_ADDR, id);
    else
        comp_int(S, symb_cell(&S->symb, id));
}

void expr_varaddr(Session *const S)
{
    varaddr(S);
    comp_int(S, S->expr.vakind);
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
            S->expr.fixdbl = true;
            comp_opb(S, O_D_FN, DF_FIX);
        } else {
            comp_tofloat(S);
            comp_call(S, K_FIX);
        }
        return;
    case 'W': /* HEX$, OCT$: a SHORT's 16 bits */
        expr_shclass(S);
        comp_toint(S);
        if (S->expr.shclass == SH_SHORT) {
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

/* The function token at comp.p (FFh n as 100h + n), comp.p past it: its XFn. */
static const XFn *xfn_at(Session *const S)
{
    uint16_t tok = *S->comp.p;
    if (tok == TK_X)
        tok = 0x100 + *++S->comp.p;
    S->comp.p++;
    for (size_t i = 0; i < countof(xfns); i++)
        if (xfns[i].tok == tok)
            return &xfns[i];
    synerr(S);
}

/* f's ( arguments ), each converted to its kind; their count after them when it varies. */
static void xfn_args(Session *const S, const XFn *const f)
{
    comp_open(S);
    uint32_t n = 0, i = 0; /* the arguments, the kind's index */
    for (;;) {
        char k = f->arg[i];
        if (k == 'R') { /* an array, n or n(): its id */
            if (comp_cur(&S->comp) != M_VAR)
                synerr(S);
            const uint16_t id = comp_var(S, 2, 0);
            if (*S->comp.p == '(') {
                if (S->comp.p[1] != ')')
                    synerr(S);
                S->comp.p += 2;
            }
            comp_int(S, id);
        } else {
            if (k == 'H') { /* a file number, its # or not */
                if (comp_cur(&S->comp) == '#')
                    S->comp.p++;
                k = 'I';
            }
            expr_compile(S);
            if (!n && f->min & XF_LEAD && S->comp.type == T_STR && k != 'S') /* a leading optional
                                                                             left out */
                k = f->arg[++i];
            xarg(S, k);
        }
        n++;
        i++;
        if (i >= f->max || !comp_optcomma(S))
            break;
    }
    if (i < (f->min & XF_MIN))
        synerr(S);
    comp_close(S);
    if ((f->min & XF_MIN) != f->max)
        comp_int(S, n);
}

/* QBasic's other functions, by xfns: f(args), then CALL k, or none. */
static void xfunc(Session *const S)
{
    const XFn *const f = xfn_at(S);
    if (f->max && ((f->min & XF_MIN) || comp_cur(&S->comp) == '('))
        xfn_args(S, f);
    else if (f->max) /* all optional, ( ) left out */
        comp_int(S, 0);
    if (f->k)
        comp_call(S, f->k);
    S->comp.type = S->expr.fixdbl ? T_DBL : f->res;
    S->expr.fixdbl = false;
    S->comp.iload = S->comp.iloadend = nullptr;
}

/* Function f's (TK_SGN on) argument converted; comp.type its result. */
static void farg(Session *const S, const uint8_t f)
{
    const uint8_t sig = fnsig[f];
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

/* LEN(r): a record's, a field's, a number's size; false: LEN's own call, nothing compiled. */
static bool lensize(Session *const S)
{
    const uint8_t *const p = S->comp.p;
    uint8_t *const out = S->comp.out;
    S->comp.p++;
    if (comp_cur(&S->comp) == '(' && comp_curn(&S->comp) == M_VAR) {
        if (ctrl_recref(S)) {
            if (comp_cur(&S->comp) == ')') {
                S->comp.p++;
                comp_int(S, field_size(S));
                return true;
            }
        } else { /* a scalar: % 2, & ! 4, # 8 */
            const uint16_t id = comp_var(S, 0, 0);
            if (!(symb_var(&S->symb, id)->info & VI_ARR)) {
                const uint32_t v = vakind(S, id);
                if (comp_cur(&S->comp) == ')' && va_kind(v) != VA_DESC) {
                    S->comp.p++;
                    comp_int(S, va_kind(v) == VA_I2 ? 2 : (uint8_t)v);
                    return true;
                }
            }
        }
    }
    S->comp.p = p;
    S->comp.out = out;
    return false;
}

/* SGN .. MID$, from cm_un's @cu_7a: f the function, TK_SGN on. */
static void func(Session *const S, const uint8_t f)
{
    if (f == TK_RND - TK_SGN && S->comp.p[1] != '(') { /* RND alone: RND(1) */
        S->comp.p++;
        comp_int(S, 1);
        farg(S, f);
        comp_call(S, K_FN + f);
        return;
    }
    S->comp.p++;
    comp_open(S);
    expr_compile(S);
    if (f >= TK_MID - TK_SGN - 2) { /* LEFT$, RIGHT$, MID$ */
        comp_comma(S);
        comp_tostring(S);
        expr_compile(S);
        comp_toaddr(S);
        uint8_t k = f;
        if (f == TK_MID - TK_SGN) {
            if (comp_optcomma(S)) {
                expr_compile(S);
                comp_toaddr(S);
            } else
                k = K_MID2 - K_FN;
        }
        comp_close(S);
        S->comp.type = T_STR;
        comp_call(S, K_FN + k);
        return;
    }
    comp_close(S);
    const uint8_t df = dfnk[f];
    if (S->comp.type == T_INT && df == DFN_STR) /* STR$ of an INT: through a DOUBLE, every digit */
        comp_todouble(S);
    else if (S->expr.dbltgt && !(S->comp.type & (T_STR | T_DBL)) && df < DFN_STR)
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

/* The code compiled from begin on as a leaf node, the code as before: one literal load (VARPTR's)
   a literal, else its bytes kept. */
static uint16_t code_leaf(Session *const S, uint8_t *const begin)
{
    const uint32_t len = (uint32_t)(S->comp.out - begin);
    if (len && fold_isliteral(*begin) && flow_opsize(begin) == len) {
        const uint8_t sh = expr_shclass(S);
        S->comp.out = begin;
        return lit_leaf(S, *begin, begin + 1, sh);
    }
    if (S->ast.ncode + len > AST_CODE)
        repl_error(S, E_OM);
    const uint16_t i = node_new(S, N_CODE);
    Node *const n = node(S, i);
    n->type = S->comp.type;
    n->sh = expr_shclass(S);
    n->at = S->ast.ncode;
    n->len = (uint16_t)len;
    for (uint32_t k = 0; k < len; k++)
        S->ast.code[S->ast.ncode++] = begin[k];
    S->comp.out = begin;
    return i;
}

/* A literal load op and its operand bytes as a leaf. */
static uint16_t lit_leaf(Session *const S, const uint8_t op, const uint8_t *const bytes, const uint8_t sh)
{
    const uint16_t i = node_new(S, N_LIT);
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

/* A variable's value: a scalar's node, a CONST's literal, or its code. */
static uint16_t var_leaf(Session *const S)
{
    uint8_t *const begin = S->comp.out;
    const uint16_t id = load(S);
    if (id == NO_ID)
        return code_leaf(S, begin);
    const uint8_t kop = symb_varx(&S->symb, id)->kop;
    if (kop)
        return lit_leaf(S, kop, symb_kval(&S->symb, id),
                        symb_var(&S->symb, id)->type == ET_SH ? SH_SHORT : SH_NONE);
    const uint16_t i = node_new(S, N_VAR);
    node(S, i)->a = id;
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
        uint8_t *const begin = S->comp.out;
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
            const uint16_t n = node_new(S, N_UN);
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
            expr_string(S);
            return code_leaf(S, begin);
        }
        if (c >= '.' && c <= '9')
            return num_leaf(S);
        if (c == TK_LEN && lensize(S))
            return code_leaf(S, begin);
        uint8_t k;
        if (c == TK_X && addr_call(S->comp.p[1], &k)) {
            S->comp.p += 2;
            comp_open(S);
            varaddr(S);
            comp_close(S);
            if (k)
                comp_call(S, k);
            S->comp.type = T_INT;
            return code_leaf(S, begin);
        }
        const uint8_t f = c - TK_SGN;
        if (f > TK_MID - TK_SGN)
            xfunc(S);
        else
            func(S, f);
        return code_leaf(S, begin);
    }
}
