/* The expression parser: precedence climbing over the token cursor, into a tree.
 *
 * # How it works
 * expr_compile parses an expression at comp.p by precedence climbing (prec), builds a tree
 * (comp/ast.h), has sema_tree check it, fold_tree fold it and cg_tree emit it. Precedences: OR 46h,
 * AND 50h, NOT 5Ah, > = < 64h, + - 78h, * / 7Bh, unary - 7Dh, ^ 7Fh. A relation is REL | DC_*.
 *
 * A function call, an element, a field or a string literal is built where it is met, by the helpers
 * here and in refs.c. expr_target compiles a store's target and its subscripts.
 *
 * # Example
 *    x% = 2 + 3 * y%     the tree + (2, * (3, y%)); the 2 loads, y% * 3 folds into one op
 *
 * # asm structure (retired/asm32/comp/expr.asm)
 * | asm | here | |
 * |---|---|---|
 * | cm_expr | expr_compile | MIRROR; D1 |
 * | cm_lexpr, cm_str1, cm_va | expr_assign, expr_string, expr_varaddr | MIRROR; actions |
 * | cm_paren, cm_subs0, cm_load, cm_lval | expr_paren, expr_target's N_TARGET, expr_load, expr_target | MIRROR |
 * | cm_shv, cm_icmp, cm_fcmp, cm_lflt | code_shclass (code.c), cg_icmp, cg_fcmp, cg_lfloat (codegen.c) | MOVED |
 * | cm_p, cm_un | parse, parse_unary | D1 |
 * | cm_sop, cm_shid, cm_iop, cm_fop2, cm_fkind, cm_str | shortop, shortid, cg_binary, fltkind (codegen.c) | MOVED; D1 |
 * | cm_right | isright (codegen.c) | MOVED |
 * | cm_nany | | D3 |
 * | cm_addr | ref_place (refs.c) | MOVED |
 * | cm_xfn, cm_xarg, cm_farg | xfn_at, xfn_node; xarg, farg (codegen.c) | D1 |
 * | g_cprec, operator indices | prectab, OP_*, P_*; a relation REL \| DC_* | MIRROR |
 * | g_ckon | expr_ktype[5]; kind end | D2 |
 * | g_cwd, g_cbk, g_vakind | Expr.dbltgt, Node.dbl, ref_place's vakind | MIRROR |
 * | g_xfn, g_fcmpk, g_lda1, g_ldan, g_dfnk, g_fnarg, g_fnsig, g_xop | builtins.c's tables; codegen.c's | MOVED |
 *
 * - D1. The asm emits as it parses; the C builds a tree. parse and parse_unary make nodes,
 *   sema_tree checks, fold_tree folds each operator whose operands are literals (through the VM's
 *   handlers, as cm_fold does), cg_tree emits with the asm's choices.
 * - D2. g_ckon is gone; the kind is Var.info's.
 * - D3. Not carried over.
 */
#include "comp/expr.h"
#include "comp/builtins.h"
#include "comp/fold.h"
#include "comp/refs.h"
#include "comp/sema.h"
#include "comp/blocks.h"
#include "comp/control.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "io/repl.h"
#include "mem/scope.h"
#include "mem/string.h"
#include "mem/symbols.h"
#include "num/double.h"
#include "num/fpmath.h"
#include "num/single.h"
#include "vm/flow.h"

const uint8_t expr_ktype[5] = {T_FLT, T_INT, T_STR, T_DBL, T_DBL};

/* Precedences: the higher binds tighter. */
enum : uint8_t {
    P_IMP = 0x40,
    P_EQV = 0x42,
    P_XOR = 0x44,
    P_OR = 0x46,
    P_AND = 0x50,
    P_NOT = 0x5A,
    P_REL = 0x64,
    P_ADD = 0x78,
    P_MOD = 0x79,
    P_IDIV = 0x7A,
    P_MUL = 0x7B,
    P_NEG = 0x7D,
    P_POW = 0x7F,
};
static const uint8_t prectab[NOPS] = {
    [OP_ADD] = P_ADD,
    [OP_SUB] = P_ADD,
    [OP_MUL] = P_MUL,
    [OP_DIV] = P_MUL,
    [OP_POW] = P_POW,
    [OP_AND] = P_AND,
    [OP_OR] = P_OR,
    [OP_GT] = P_REL,
    [OP_EQ] = P_REL,
    [OP_LT] = P_REL,
    [OP_IDIV] = P_IDIV,
    [OP_MOD] = P_MOD,
    [OP_XOR] = P_XOR,
    [OP_EQV] = P_EQV,
    [OP_IMP] = P_IMP,
};
static const uint8_t relbit[] = {DC_GT, DC_EQ, DC_LT}; /* from OP_GT */

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

static Node *parse(Comp *C, uint8_t p);
static Node *parse_unary(Comp *C);
static Node *var_leaf(Comp *C);
static Node *str_leaf(Comp *C);

void expr_compile(Comp *const C) { comp_step(C, expr_tree(C, false)); }

Node *expr_tree(Comp *const C, const bool dbl)
{
    const bool was = C->expr->dbltgt;
    C->expr->dbltgt = dbl;
    Node *const root = parse(C, 0);
    C->expr->dbltgt = was;
    return root;
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
static uint8_t relation(Comp *const C, uint8_t op)
{
    uint8_t rel = REL;

    for (;;) {
        rel |= relbit[op - OP_GT];
        const uint8_t c = comp_curn(C);

        if (c < TK_GT || c > TK_LT)
            return rel;

        op = c - TK_PLUS;
    }
}

/* cm_p: the operand and the operators after it that bind tighter than precedence p. */
static Node *parse(Comp *const C, const uint8_t p)
{
    if (++C->depth > COMP_MAXDEPTH)
        fault_error(C->fault, E_OM);

    Node *left = parse_unary(C);
    uint8_t op;

    while (operator_of(comp_cur(C), &op) && p < prectab[op]) {
        uint8_t rel = op;

        if (op >= OP_GT && op <= OP_LT)
            rel = relation(C, op);
        else
            C->p++;

        Node *const right = parse(C, prectab[op]);
        Node *const n = ast_new(C->ast, N_BIN);
        *n = (Node){
            .kind = N_BIN,
            .op = rel,
            .dbl = C->expr->dbltgt,
            .a = left,
            .b = right,
            .end = C->p,
        };

        left = n;
    }

    C->depth--;
    return left;
}

void expr_paren(Comp *const C)
{
    comp_open(C);
    expr_compile(C);
    comp_close(C);
}

void expr_string(Comp *const C) { comp_step(C, str_leaf(C)); }

void expr_load(Comp *const C) { comp_step(C, var_leaf(C)); }

/* A record's field r the target: its address emitted. */
static void target_field(Comp *const C, Node *const r)
{
    const RecRef rec = r->rec;

    if (rec.et >= ET_REC)
        mismatch(C);

    comp_step(C, r);
    C->tgt = rec.off;
    C->tgtsubs = TGT_FIELD;
    C->ctrl->fieldet = rec.et;
    C->ctrl->fieldfix = rec.fix;
    C->tgtinfo = comp_etkind[rec.et];
}

void expr_target(Comp *const C)
{
    Node *const r = comp_cur(C) == M_VAR ? ref_record(C) : nullptr;

    if (r != nullptr) {
        target_field(C, r);
        return;
    }

    comp_notconst(C);
    const uint16_t id = comp_name(C);

    if (!(symb_var(C->symb, id)->info & VI_ARR)) {
        comp_target(C, id);
        return;
    }

    Node *const t = ast_new(C->ast, N_TARGET); /* a step now: an error's code keeps the
                                                   subscripts before it */
    t->id = id;
    ast_step(C->ast, t);
    Node *last = nullptr;
    uint8_t n = 0;
    comp_open(C);

    do {
        n++;
        Node *const e = expr_tree(C, false);
        comp_check(C, e);

        if (ast_type(e) & T_STR)
            mismatch(C);

        Node *const cv = ast_new(C->ast, N_CONV);
        *cv = (Node){
            .kind = N_CONV,
            .op = CV_INT,
            .type = T_INT,
            .a = e,
            .end = C->p,
        };

        ast_link(t, &last, cv);
    } while (comp_optcomma(C));

    comp_close(C);
    comp_target(C, id); /* after the subscripts: a call in them has its own target */
    C->tgtsubs = n;
}

void expr_varaddr(Comp *const C)
{
    uint32_t vk;
    comp_step(C, expr_addr(C, ref_place(C, &vk), 0));
    comp_int(C, vk);
}

/* The function token at comp.p (FFh n as 100h + n), comp.p past it: its XFn. */
static const XFn *xfn_at(Comp *const C)
{
    const uint16_t tok = *C->p == TK_X ? 0x100 + C->p[1] : *C->p;

    for (size_t i = 0; i < fns_nx; i++) {
        if (fns_x[i].tok == tok) {
            C->p += tok >= 0x100 ? 2 : 1;
            return &fns_x[i];
        }
    }

    synerr(C); /* at the token: not a function */
}

/* VARPTR, VARPTR$, VARSEG, SSEG, SADD (v) after FFh: its CALL in k, 0 none; false: not one. */
static bool addr_call(const uint8_t tok, uint8_t *const k)
{
    switch (tok) {
    case TK1_VARPTR:
        *k = 0;
        return true;

    case TK1_VARSEG:
    case TK1_SSEG:
        *k = K_VSEG;
        return true;

    case TK1_SADD:
        *k = K_SADD;
        return true;

    case TK1_VARPTRS:
        *k = K_VARPS;
        return true;

    default:
        return false;
    }
}

static Node *lit_leaf(Comp *C, uint8_t op, const uint8_t *bytes, uint8_t sh);

Node *expr_arg(Comp *const C)
{
    const bool was = C->expr->dbltgt;
    C->expr->dbltgt = false;
    Node *const n = parse(C, 0);
    C->expr->dbltgt = was;
    return n;
}

static Node *args_node(Comp *const C, const NodeKind kind)
{
    Node *const n = ast_new(C->ast, kind);
    n->b = nullptr;
    return n;
}

/* Array id's element: ( e {, e} ). */
static Node *elem_node(Comp *const C, const uint16_t id)
{
    Node *const n = args_node(C, N_ELEM);
    n->id = id;
    n->type = expr_ktype[symb_var(C->symb, id)->info & (SK_DBL | VK_MASK)];
    expr_args(C, n);
    return n;
}

void expr_args(Comp *const C, Node *const n)
{
    Node *last = nullptr;
    comp_open(C);

    do
        ast_link(n, &last, expr_arg(C));
    while (comp_optcomma(C));

    comp_close(C);
}

/* A string literal after its quote: S.LD.TXT len, its text's Addr. */
static Node *str_leaf(Comp *const C)
{
    const uint8_t *const start = C->p;
    const uint8_t *q = start;
    uint8_t b[5];
    b[0] = (uint8_t)strg_span(&q, '"', '"');
    store32(b + 1, addr_of(start));
    C->p = strg_skipq(q);
    return lit_leaf(C, O_S_LD_TXT, b, SH_NONE);
}

/* SGN .. MID$ (f, TK_SGN on): (e), LEFT$ RIGHT$ (s, n), MID$ (s, n [, n]); RND alone. */
static Node *fn_node(Comp *const C, const uint8_t f)
{
    Node *const n = args_node(C, N_FN);
    n->op = f;
    n->dbl = C->expr->dbltgt;
    C->p++;

    if (f == TK_RND - TK_SGN && *C->p != '(') { /* RND alone: RND(1) */
        n->type = T_FLT;
        return n;
    }

    Node *last = nullptr;
    comp_open(C);
    ast_link(n, &last, expr_arg(C));

    if (f >= TK_MID - TK_SGN - 2) {
        comp_comma(C);
        ast_link(n, &last, expr_arg(C));

        if (f == TK_MID - TK_SGN && comp_optcomma(C))
            ast_link(n, &last, expr_arg(C));
    }

    comp_close(C);
    n->type = fns_type(f, ast_type(n->b), C->expr->dbltgt);
    n->end = C->p;
    return n;
}

/* QBasic's other functions, by fns_x: f(args) each of its kind, their count when it varies. */
static Node *xfn_node(Comp *const C)
{
    const XFn *const f = xfn_at(C);
    Node *const n = args_node(C, N_XFN);
    n->op = (uint8_t)(f - fns_x);
    n->type = f->res;

    if (!f->max)
        return n;
    if (!(f->min & XF_MIN) && comp_cur(C) != '(') { /* all optional, ( ) left out */
        n->mode = XM_NONE;
        return n;
    }

    Node *last = nullptr;
    comp_open(C);

    for (uint32_t i = 0;; i++) {
        char k = f->arg[i];
        Node *a;

        if (k == 'R') { /* an array, n or n(): its id */
            if (comp_cur(C) != M_VAR)
                synerr(C);

            const uint16_t id = comp_var(C, 2, 0);

            if (*C->p == '(') {
                if (C->p[1] != ')')
                    synerr(C);

                C->p += 2;
            }

            a = expr_intleaf(C, id, SH_NONE);
        } else {
            if (k == 'H') { /* a file number, its # or not */
                if (comp_cur(C) == '#')
                    C->p++;

                k = 'I';
            }

            a = expr_arg(C);

            if (n->nargs == 0 && f->min & XF_LEAD && ast_type(a) == T_STR && k != 'S')
                k = f->arg[++i];                  /* a leading optional left out */
            if (k == 'G' && ast_type(a) == T_DBL) /* FIX of a DOUBLE: D.FN's */
                n->type = T_DBL;
        }

        a->conv = (uint8_t)k;
        ast_link(n, &last, a);

        if (i + 1 >= f->max || !comp_optcomma(C)) {
            if (i + 1 < (f->min & XF_MIN))
                synerr(C);

            break;
        }
    }

    comp_close(C);
    n->mode = (f->min & XF_MIN) != f->max ? XM_COUNT : XM_ARGS;
    n->end = C->p;
    return n;
}

Node *expr_addr(Comp *const C, Node *const at, const uint8_t k)
{
    Node *const n = ast_new(C->ast, N_ADDR);
    n->a = at;
    n->op = k;
    n->type = T_INT;
    return n;
}

/* A literal load op and its operand bytes as a leaf. */
static Node *lit_leaf(Comp *const C, const uint8_t op, const uint8_t *const bytes, const uint8_t sh)
{
    Node *const i = ast_new(C->ast, N_LIT);
    Node *const n = i;
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
Node *expr_intleaf(Comp *const C, const uint32_t v, const uint8_t sh)
{
    uint8_t b[4];
    store32(b, v);
    const uint8_t op = v > 0xFFFF ? O_I_LD_I32 : v > 0xFF ? O_I_LD_I16 : O_I_LD_U8;
    return lit_leaf(C, op, b, sh);
}

/* A variable's value at comp.p: a FUNCTION's call, a record's field, an element, a CONST's
   literal, a scalar. */
static Node *var_leaf(Comp *const C)
{
    const Name *const nm = symb_name(C->symb, comp_nameof(C->p));

    if (nm->flags & (NM_FUNC | NM_FN)) { /* in FUNCTION f, f alone is its result */
        if (!nm->proc)
            return ref_undef(C);
        if ((*comp_skipname(C->p) == '(' || nm->proc != C->proc) &&
            !(blok_proc(C->blok, nm->proc)->flags & PF_RECRES)) { /* a record's: ref_record's */
            const uint8_t *const name = C->p;
            uint8_t idx;
            const Proc *const pr = ref_proc(C, &idx);
            return ref_call(C, name, idx, pr, ARGS_PAREN);
        }
    }

    Node *const r = ref_record(C);

    if (r != nullptr)
        return ref_field(C, r);

    const uint16_t id = comp_name(C);

    if (symb_var(C->symb, id)->info & VI_ARR)
        return elem_node(C, id);

    const uint8_t kop = symb_varx(C->symb, id)->kop;

    if (kop)
        return lit_leaf(
            C,
            kop,
            symb_kval(C->symb, id),
            symb_var(C->symb, id)->type == ET_SH ? SH_SHORT : SH_NONE
        );

    Node *const i = ast_new(C->ast, N_VAR);
    i->id = id;
    i->type = expr_ktype[symb_var(C->symb, id)->info & (SK_DBL | VK_MASK)];
    return i;
}

/* A number's literal: a DOUBLE's D.LD.K, else F.LD.F32. */
static Node *num_leaf(Comp *const C)
{
    uint8_t kind;
    const double d = dubl_in(C->fault, &C->p, &kind);
    uint8_t b[8];

    if (kind == DK_DBL) {
        const uint64_t q = (Cell){.d = d}.raw;

        store32(b, (uint32_t)q);
        store32(b + 4, (uint32_t)(q >> 32));
        return lit_leaf(C, O_D_LD_K, b, SH_NONE);
    }

    store32(b, sngl_from_dbl(d));
    return lit_leaf(C, O_F_LD_F32, b, SH_NONE);
}

/* cm_un: an operand: a variable, an element, a literal, ( expr ), - NOT +, a function. */
static Node *parse_unary(Comp *const C)
{
    for (;;) {
        const uint8_t c = comp_cur(C);

        switch (c) {
        case M_VAR:
            return var_leaf(C);

        case M_INT: {
            const uint32_t v = load32(C->p + 1);
            C->p += INT_TOKEN_SIZE;
            return expr_intleaf(C, v, v <= INT16_MAX ? SH_LIT : SH_NONE);
        }
        case '(': { /* its own: not for a DOUBLE target */
            comp_open(C);
            const bool was = C->expr->dbltgt;
            C->expr->dbltgt = false;
            Node *const n = parse(C, 0);
            C->expr->dbltgt = was;
            comp_close(C);
            return n;
        }
        case TK_MINUS:
        case TK_NOT: {
            C->p++;
            Node *const a = parse(C, c == TK_MINUS ? P_NEG : P_NOT);
            Node *const n = ast_new(C->ast, N_UN);
            n->op = c;
            n->a = a;
            n->end = C->p;
            return n;
        }
        case TK_PLUS:
            C->p++;
            continue;

        case '"':
            C->p++;
            return str_leaf(C);
        }

        if (c >= '.' && c <= '9')
            return num_leaf(C);

        uint32_t size;

        if (c == TK_LEN && ref_len(C, &size))
            return expr_intleaf(C, size, SH_LIT);

        uint8_t k;

        if (c == TK_X && addr_call(C->p[1], &k)) {
            C->p += 2;
            comp_open(C);
            uint32_t vk;
            Node *const at = ref_place(C, &vk);
            comp_close(C);

            if (k == K_VARPS) {
                Node *const n = expr_addr(C, at, k);
                n->type = T_STR;
                n->num = ref_vtype(C, at);
                return n;
            }

            return k || !ast_islit(at) ? expr_addr(C, at, k) : at;
        }

        const uint8_t f = c - TK_SGN;
        return f > TK_MID - TK_SGN ? xfn_node(C) : fn_node(C, f);
    }
}
