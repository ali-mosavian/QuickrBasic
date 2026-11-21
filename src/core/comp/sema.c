/* The semantic pass: names resolved and types checked on a parsed tree, before it is emitted.
 *
 * # How it works
 * sema_tree resolves a tree's names to the facts codegen needs (Node.var, par, jump) and checks its
 * types, raising every error, so codegen.c gets a tree it compiles without one. The walk is
 * codegen's, so an error's place (comp.p) is where codegen's was.
 *
 * # Example
 *    a$ + 1     Type mismatch, raised here, not while emitting
 *
 * # asm structure
 * No asm counterpart: the asm checks types while it emits.
 */
#include "comp/sema.h"
#include "comp/control.h"
#include "comp/dfa.h"
#include "comp/builtins.h"
#include "io/repl.h"
#include "mem/symbols.h"

[[noreturn]]
static void mismatch(Comp *const C)
{
    fault_error(C->fault, E_TM);
}

static void check(Comp *C, Node *n);

static void own(Comp *const C, Node *const n)
{
    const Var *const v = symb_var(C->symb, n->id);
    n->vtype = v->type;
    n->vinfo = v->info;
}

/* The facts gen needs of n's names. */
static void resolve(Comp *const C, Node *const n)
{
    switch (n->kind) {
    case N_VAR:
    case N_ELEM:
    case N_REF:
    case N_TARGET:
    case N_STORE:
    case N_OPID:
    case N_CASECMP:
        own(C, n);
        n->par = comp_ispar(C, n->id);
        break;

    case N_ARG:
        if (n->op == PA_VALUE)
            own(C, n);

        break;

    case N_CALL:
        if (n->mode == CALL_UNDEF) {
            own(C, n);
            n->par = comp_ispar(C, n->id);
        } else if (n->mode == CALL_PROC)
            n->jump = ctrl_jump(C, blok_proc(C->blok, n->num)->entry);

        break;

    default:
        break;
    }
}

static bool is_str(const Node *const n) { return ast_type(n) & T_STR; }

/* The value must be a number. */
static void number(Comp *const C, Node *const n)
{
    if (is_str(n))
        mismatch(C);
}

/* The value must be a string. */
static void string(Comp *const C, Node *const n)
{
    if (!is_str(n))
        mismatch(C);
}

/* cg_as's: n as a number. */
static void as_number(Comp *const C, Node *const n)
{
    check(C, n);
    number(C, n);
}

static void args_as_numbers(Comp *const C, Node *const n)
{
    for (Node *a = n->b; a; a = a->next)
        as_number(C, a);
}

static void binary(Comp *const C, Node *const n)
{
    const uint8_t rel = n->op;

    if (ast_work(n) == W_STR) {
        check(C, n->a);
        check(C, n->b);
        C->p = n->end;
        string(C, n->b);

        if (rel != OP_ADD && !ast_isrel(rel))
            mismatch(C);

        return;
    }

    as_number(C, n->a);

    if (is_str(n->b)) {
        check(C, n->b);
        C->p = n->end;
        mismatch(C);
    }

    as_number(C, n->b);
    C->p = n->end;
}

static void unary(Comp *const C, Node *const n)
{
    check(C, n->a);
    C->p = n->end;
    number(C, n->a);
}

/* farg's: SGN .. CHR$'s argument, of type t, as f takes it: FRE's either. */
static void fn_arg(Comp *const C, const uint8_t f, const uint8_t t)
{
    const uint8_t k = fns_sig[f] & FA_MASK;

    if (k != FA_Y && (k == FA_S ? !(t & T_STR) : t & T_STR))
        mismatch(C);
}

static void fn(Comp *const C, Node *const n)
{
    const uint8_t f = n->op;

    if (!n->nargs) /* RND alone: of 1 */
        return;

    Node *const a = n->b;
    check(C, a);
    C->p = n->end;

    if (f >= TK_MID - TK_SGN - 2) { /* LEFT$, RIGHT$, MID$ (s, n [, n]) */
        string(C, a);

        for (Node *b = a->next; b; b = b->next) {
            check(C, b);
            number(C, b);
        }

        return;
    }

    const uint8_t df = fns_dfnk[f];
    uint8_t t = ast_type(a);

    if ((t == T_INT && df == DFN_STR) || (n->dbl && !(t & (T_STR | T_DBL)) && df < DFN_STR))
        t = T_DBL;
    if (t == T_DBL && df != DFN_NONE) /* D.FN's, STR$'s */
        return;

    fn_arg(C, f, t);
}

/* xarg's: an argument of kind k. */
static void xfn(Comp *const C, Node *const n)
{
    for (Node *a = n->b; a; a = a->next) {
        check(C, a);
        C->p = n->end;

        switch (a->conv) {
        case 'R':
        case 'X':
            break; /* an array's id; a string or a number */
        case 'S':
            string(C, a);
            break;

        case 'G': /* FIX of a DOUBLE: D.FN's */
            if (ast_type(a) != T_DBL)
                number(C, a);

            break;

        default:
            number(C, a); /* D W I F */
        }
    }
}

static void arg(Comp *const C, Node *const n)
{
    if (!n->a)
        return; /* an array or a record whole */
    check(C, n->a);

    if (n->op != PA_VALUE)
        return;

    const uint8_t info = symb_var(C->symb, n->id)->info;

    if ((info & (SK_DBL | VK_MASK)) == VK_STR)
        string(C, n->a);
    else
        number(C, n->a);
}

static void conv(Comp *const C, Node *const n)
{
    check(C, n->a);
    C->p = n->end;

    if (n->op == CV_STR)
        string(C, n->a);
    else
        number(C, n->a);
}

static void check(Comp *const C, Node *const n)
{
    resolve(C, n);

    switch (n->kind) {
    case N_UN:
        unary(C, n);
        break;

    case N_BIN:
        binary(C, n);
        break;

    case N_ELEM:
        args_as_numbers(C, n);
        break;

    case N_FN:
        fn(C, n);
        break;

    case N_XFN:
        xfn(C, n);
        break;

    case N_REF:
        if (n->op == O_RA_ADDR)
            args_as_numbers(C, n);

        break;

    case N_FIELD:
    case N_ADDR:
        check(C, n->a);
        break;

    case N_ARG:
        arg(C, n);
        break;

    case N_CALL:
        for (Node *a = n->b; a; a = a->next)
            check(C, a);
        if (n->a != nullptr) /* a record result's place */
            check(C, n->a);

        break;

    case N_CONV:
        conv(C, n);
        break;

    case N_TARGET:
        args_as_numbers(C, n);
        break;

    case N_STORE:
        check(C, n->a);
        break;

    default:
        break; /* N_LIT, N_VAR, the statements' nodes */
    }
}

void sema_tree(Comp *const C, Node *const n) { check(C, n); }

void sema_resolve(Comp *const C, Node *const n) { resolve(C, n); }
