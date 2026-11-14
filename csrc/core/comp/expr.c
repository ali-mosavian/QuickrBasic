/* The expression compiler (comp/expr.asm). Precedences: OR 46h, AND 50h, NOT 5Ah, > = < 64h,
   + - 78h, * / 7Bh, unary - 7Dh, ^ 7Fh. */
#include "comp/expr.h"
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
static void gen_elem(Session *S, const Node *n);
static void gen_fn(Session *S, const Node *n);
static void gen_xfn(Session *S, const Node *n);
static void gen_ref(Session *S, const Node *n);
static void gen_field(Session *S, const Node *n);
static void gen_addr(Session *S, const Node *n);
static void gen_arg(Session *S, const Node *n);
static void gen_call(Session *S, const Node *n);
static uint16_t addr_node(Session *S, uint16_t at, uint8_t k);
static uint16_t var_leaf(Session *S);

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
    gen(S, root);
    S->comp.p = end;
    S->ast.nnodes = nodes;
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

uint16_t ast_new(Session *const S, const NodeKind kind)
{
    if (S->ast.nnodes >= AST_NODES)
        repl_error(S, E_OM);
    const uint16_t i = S->ast.nnodes++;
    S->ast.node[i] = (Node){.kind = kind};
    return i;
}

Node *ast_node(Session *const S, const uint16_t i) { return &S->ast.node[i]; }

void ast_link(Session *const S, const uint16_t n, uint16_t *const last, const uint16_t a)
{
    ast_node(S, a)->next = NO_NODE;
    if (*last == NO_NODE)
        ast_node(S, n)->b = a;
    else
        ast_node(S, *last)->next = a;
    *last = a;
    ast_node(S, n)->nargs++;
}

static Node *node(Session *const S, const uint16_t i) { return ast_node(S, i); }

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
        const uint16_t n = ast_new(S, N_BIN);
        *node(S, n) = (Node){.kind = N_BIN, .op = rel, .dbl = S->expr.dbltgt, .a = left, .b = right,
                             .end = S->comp.p};
        left = n;
    }
    S->comp.depth--;
    return left;
}

static uint8_t vtype(Session *S, uint16_t i);

/* A DOUBLE either side; or for a DOUBLE target ^, and unless of constants / or a SINGLE's. */
static bool wants_double(const Node *const n, const uint8_t lt, const uint8_t rt, const bool bothk)
{
    if (lt == T_DBL || rt == T_DBL)
        return true;
    if (!n->dbl)
        return false;
    return n->op == OP_POW || (!bothk && (n->op == OP_DIV || lt == T_FLT || rt == T_FLT));
}

/* How binary node n computes: its operands' type, W_* below. */
enum : uint8_t { W_INT, W_FLT, W_DBL, W_POW, W_STR };

static uint8_t work(Session *const S, const Node *const n)
{
    const uint8_t rel = n->op;
    if (is_logic(rel) || is_xop(rel))
        return W_INT;
    const uint8_t lt = vtype(S, n->a), rt = vtype(S, n->b);
    if (lt & T_STR)
        return W_STR;
    const bool bothk = rel != OP_POW && is_lit(S, n->a) && is_lit(S, n->b);
    if (wants_double(n, lt, rt, bothk))
        return W_DBL;
    if (rel == OP_POW)
        return W_POW;
    return lt == T_INT && rt == T_INT && rel != OP_DIV ? W_INT : W_FLT;
}

/* Node i's value's type, nothing compiled; a mismatch is gen's to raise. */
static uint8_t vtype(Session *const S, const uint16_t i)
{
    const Node *const n = node(S, i);
    switch (n->kind) {
    case N_VAR: {
        const uint8_t info = symb_var(&S->symb, n->id)->info;
        return expr_ktype[info & (SK_DBL | VK_MASK)];
    }
    case N_UN: return n->op == TK_NOT ? T_INT : vtype(S, n->a);
    case N_BIN:
        if (is_rel(n->op))
            return T_INT;
        switch (work(S, n)) {
        case W_INT: return T_INT;
        case W_DBL: return T_DBL;
        case W_STR: return T_STR;
        default: return T_FLT;
        }
    default: return n->type;
    }
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
    if (t == T_FLT && vtype(S, i) == T_INT && one_load(S, n, T_FLT, left, &op, b)) {
        S->expr.shend = nullptr;
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
            if (is_rel(rel)) {
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
    *rsh = expr_shclass(S);
    if (is_rel(rel))
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
    const uint8_t w = work(S, n);
    uint8_t lsh = SH_NONE, rsh = SH_NONE;
    if (w == W_STR) {
        gen(S, n->a);
        gen(S, n->b);
        S->comp.p = n->end;
        if (!(S->comp.type & T_STR))
            mismatch(S);
        if (rel == OP_ADD)
            comp_op(S, O_S_CAT);
        else if (!is_rel(rel))
            mismatch(S);
        else
            compare(S, O_S_CMP, 0, nullptr, rel & REL_MASK);
        S->expr.sh = SH_NONE;
        S->expr.shend = S->comp.out;
        return;
    }
    const uint8_t lt = w == W_INT ? T_INT : w == W_DBL ? T_DBL : T_FLT;
    gen_as(S, n->a, lt, true);
    lsh = expr_shclass(S);
    if (vtype(S, n->b) & T_STR) {
        gen(S, n->b);
        S->comp.p = n->end;
        mismatch(S);
    }
    switch (w) {
    case W_DBL:
        gen_as(S, n->b, T_DBL, false);
        rsh = expr_shclass(S);
        S->comp.p = n->end;
        S->comp.iload = S->comp.iloadend = nullptr;
        if (rel == OP_POW)
            comp_op(S, O_D_POW);
        else if (is_rel(rel)) {
            comp_opb(S, O_D_CMP, rel & REL_MASK);
            S->comp.type = T_INT;
        } else
            comp_op(S, O_D_ADD + rel);
        break;
    case W_POW:
        gen_as(S, n->b, T_FLT, false);
        rsh = expr_shclass(S);
        S->comp.p = n->end;
        comp_call(S, K_FPOW);
        S->comp.type = T_FLT;
        break;
    default:
        if (is_xop(rel)) {
            gen_as(S, n->b, T_INT, false);
            rsh = expr_shclass(S);
            S->comp.p = n->end;
            comp_op(S, xop[rel - OP_IDIV]);
            S->comp.type = T_INT;
        } else {
            numeric(S, is_logic(rel) ? rel - 1 : rel, n->b, lt, &rsh); /* AND, OR: O_I_ADD's 4th, 5th */
            S->comp.p = n->end;
        }
    }
    const uint8_t sh = shortop(rel, lsh, rsh);
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
    default: /* N_VAR */
        loadvar(S, n->id);
        return;
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

void expr_gen(Session *const S, const uint16_t n) { gen(S, n); }

void expr_emit(Session *const S, const uint16_t root, const uint16_t mark)
{
    const uint8_t *const end = S->comp.p;
    fold_tree(S, root);
    gen(S, root);
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
    S->comp.type = T_STR;
    const uint8_t *const start = S->comp.p;
    const uint8_t *q = start;
    const uint32_t len = strg_span(&q, '"', '"');
    comp_opb(S, O_S_LD_TXT, (uint8_t)len);
    comp_dword(S, addr_of(start));
    S->comp.p = strg_skipq(q);
}

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
        expr_shortid(S, id);
    } else if (k == VK_STR) {
        S->comp.type = T_STR;
        comp_opid(S, O_S_LD_V, id);
    } else {
        S->comp.type = T_FLT;
        comp_opid(S, O_F_LD_V, id);
    }
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
    S->expr.shend = nullptr;
    expr_shortid(S, id);
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

/* SGN .. MID$'s result, its argument's type at: as func compiles it. */
static uint8_t fn_type(const uint8_t f, const uint8_t at, const bool dbl)
{
    if (f >= TK_MID - TK_SGN - 2)
        return T_STR;
    const uint8_t df = dfnk[f];
    const bool todbl = (at == T_INT && df == DFN_STR) || (dbl && !(at & (T_STR | T_DBL)) && df < DFN_STR);
    if ((at == T_DBL || todbl) && df != DFN_NONE)
        return df == DFN_STR ? T_STR : df == DF_SGN ? T_INT : T_DBL;
    if (f == TK_VAL - TK_SGN)
        return T_DBL;
    const uint8_t res = fnsig[f] & FR_MASK;
    return res == FR_S ? T_STR : res;
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
    node(S, n)->type = fn_type(f, vtype(S, node(S, n)->b), S->expr.dbltgt);
    node(S, n)->end = S->comp.p;
    return n;
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
    const uint8_t df = dfnk[f];
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

/* QBasic's other functions, by xfns: f(args) each of its kind, their count when it varies. */
static uint16_t xfn_node(Session *const S)
{
    const XFn *const f = xfn_at(S);
    const uint16_t n = args_node(S, N_XFN);
    node(S, n)->op = (uint8_t)(f - xfns);
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
            if (node(S, n)->nargs == 0 && f->min & XF_LEAD && vtype(S, a) == T_STR && k != 'S')
                k = f->arg[++i]; /* a leading optional left out */
            if (k == 'G' && vtype(S, a) == T_DBL) /* FIX of a DOUBLE: D.FN's */
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

static void gen_xfn(Session *const S, const Node *const n)
{
    const XFn *const f = &xfns[n->op];
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
    S->comp.type = S->expr.fixdbl ? T_DBL : f->res;
    S->expr.fixdbl = false;
    S->comp.iload = S->comp.iloadend = nullptr;
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
            return k || !is_lit(S, at) ? addr_node(S, at, k) : at;
        }
        const uint8_t f = c - TK_SGN;
        return f > TK_MID - TK_SGN ? xfn_node(S) : fn_node(S, f);
    }
}
