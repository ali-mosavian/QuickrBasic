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
#include "session.h"
#include "comp/control.h"
#include "comp/dfa.h"
#include "comp/builtins.h"
#include "io/repl.h"
#include "mem/symbols.h"

[[noreturn]]
static void mismatch(Session *const S)
{
    repl_error(S, E_TM);
}

static void check(Session *S, Node *n);

/* The facts gen needs of n's names, as they stand now: nothing moves them before gen. */
static void resolve(Session *const S, Node *const n)
{
    switch (n->kind) {
    case N_VAR:
    case N_ELEM:
    case N_REF:
    case N_TARGET:
    case N_STORE:
        n->var = symb_var(&S->symb, n->id);
        n->par = comp_ispar(S, n->id);
        break;

    case N_ARG:
        if (n->op == PA_VALUE)
            n->var = symb_var(&S->symb, n->id);

        break;

    case N_CALL:
        if (n->mode == CALL_UNDEF) {
            n->var = symb_var(&S->symb, n->id);
            n->par = comp_ispar(S, n->id);
        } else
            n->jump = ctrl_jump(S, blok_proc(&S->blok, n->op)->entry);

        break;

    default:
        break;
    }
}

static bool is_str(const Node *const n) { return ast_type(n) & T_STR; }

/* The value must be a number. */
static void number(Session *const S, Node *const n)
{
    if (is_str(n))
        mismatch(S);
}

/* The value must be a string. */
static void string(Session *const S, Node *const n)
{
    if (!is_str(n))
        mismatch(S);
}

/* cg_as's: n as a number. */
static void as_number(Session *const S, Node *const n)
{
    check(S, n);
    number(S, n);
}

static void args_as_numbers(Session *const S, Node *const n)
{
    for (Node *a = n->b; a; a = a->next)
        as_number(S, a);
}

static void binary(Session *const S, Node *const n)
{
    const uint8_t rel = n->op;

    if (ast_work(n) == W_STR) {
        check(S, n->a);
        check(S, n->b);
        S->comp.p = n->end;
        string(S, n->b);

        if (rel != OP_ADD && !ast_isrel(rel))
            mismatch(S);

        return;
    }

    as_number(S, n->a);

    if (is_str(n->b)) {
        check(S, n->b);
        S->comp.p = n->end;
        mismatch(S);
    }

    as_number(S, n->b);
    S->comp.p = n->end;
}

static void unary(Session *const S, Node *const n)
{
    check(S, n->a);
    S->comp.p = n->end;
    number(S, n->a);
}

/* farg's: SGN .. CHR$'s argument, of type t, as f takes it: FRE's either. */
static void fn_arg(Session *const S, const uint8_t f, const uint8_t t)
{
    const uint8_t k = fns_sig[f] & FA_MASK;

    if (k != FA_Y && (k == FA_S ? !(t & T_STR) : t & T_STR))
        mismatch(S);
}

static void fn(Session *const S, Node *const n)
{
    const uint8_t f = n->op;

    if (!n->nargs) /* RND alone: of 1 */
        return;

    Node *const a = n->b;
    check(S, a);
    S->comp.p = n->end;

    if (f >= TK_MID - TK_SGN - 2) { /* LEFT$, RIGHT$, MID$ (s, n [, n]) */
        string(S, a);

        for (Node *b = a->next; b; b = b->next) {
            check(S, b);
            number(S, b);
        }

        return;
    }

    const uint8_t df = fns_dfnk[f];
    uint8_t t = ast_type(a);

    if ((t == T_INT && df == DFN_STR) || (n->dbl && !(t & (T_STR | T_DBL)) && df < DFN_STR))
        t = T_DBL;
    if (t == T_DBL && df != DFN_NONE) /* D.FN's, STR$'s */
        return;

    fn_arg(S, f, t);
}

/* xarg's: an argument of kind k. */
static void xfn(Session *const S, Node *const n)
{
    for (Node *a = n->b; a; a = a->next) {
        check(S, a);
        S->comp.p = n->end;

        switch (a->conv) {
        case 'R':
        case 'X':
            break; /* an array's id; a string or a number */
        case 'S':
            string(S, a);
            break;

        case 'G': /* FIX of a DOUBLE: D.FN's */
            if (ast_type(a) != T_DBL)
                number(S, a);

            break;

        default:
            number(S, a); /* D W I F */
        }
    }
}

static void arg(Session *const S, Node *const n)
{
    if (!n->a)
        return; /* an array or a record whole */
    check(S, n->a);

    if (n->op != PA_VALUE)
        return;

    const uint8_t info = symb_var(&S->symb, n->id)->info;

    if ((info & (SK_DBL | VK_MASK)) == VK_STR)
        string(S, n->a);
    else
        number(S, n->a);
}

static void conv(Session *const S, Node *const n)
{
    check(S, n->a);
    S->comp.p = n->end;

    if (n->op == CV_STR)
        string(S, n->a);
    else
        number(S, n->a);
}

static void check(Session *const S, Node *const n)
{
    resolve(S, n);

    switch (n->kind) {
    case N_UN:
        unary(S, n);
        break;

    case N_BIN:
        binary(S, n);
        break;

    case N_ELEM:
        args_as_numbers(S, n);
        break;

    case N_FN:
        fn(S, n);
        break;

    case N_XFN:
        xfn(S, n);
        break;

    case N_REF:
        if (n->op == O_RA_ADDR)
            args_as_numbers(S, n);

        break;

    case N_FIELD:
    case N_ADDR:
        check(S, n->a);
        break;

    case N_ARG:
        arg(S, n);
        break;

    case N_CALL:
        for (Node *a = n->b; a; a = a->next)
            check(S, a);

        break;

    case N_CONV:
        conv(S, n);
        break;

    case N_TARGET:
        args_as_numbers(S, n);
        break;

    case N_STORE:
        check(S, n->a);
        break;

    default:
        break; /* N_LIT, N_VAR, N_KCALL, N_COUNT */
    }
}

void sema_tree(Session *const S, Node *const n) { check(S, n); }

/* Each node of the list from n. */
static void resolve_list(Session *const S, Node *n)
{
    for (; n; n = n->next)
        sema_resolve(S, n);
}

void sema_resolve(Session *const S, Node *const n)
{
    resolve(S, n);
    resolve_list(S, n->a);
    resolve_list(S, n->b);
}
