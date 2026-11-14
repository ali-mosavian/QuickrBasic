/* The compiler: a token body to stack-machine code, one statement at a time.
 *
 * # How it works
 * A statement is run by the generated automaton (comp/dfa.h): each state's entry for the byte at
 * the cursor steps past a token, goes on at the statement's end, or runs an action (A_*) or CALL
 * K_*. Each makes the statement's steps (comp/ast.h): checked as made, generated in order at
 * comp_flush, when the statement ends. The token cursor is comp.p. The helpers here also take
 * any cursor as `const uint8_t **p`. A body is in the arena (comp.cbuf or the program store), so a
 * string literal's operand is addr_of() its text.
 *
 * Errors: while comp.busy, repl_error(code) calls comp_fail(code), which ends the line's code with
 * ERR code dg at state, EOL and longjmps to comp_text's setjmp; comp_text then returns normally.
 * The error is raised when the code runs.
 *
 * Each action parses its statement's operands (expr.c, refs.c) and emits through code.c. A store
 * converts the value to its target's type (comp_store). Statement kinds with blocks, calls or
 * records go to control.c; declarations were entered when the line was (declare.c).
 *
 * # Example
 *    10 FOR i = 1 TO 3      act_for: the bounds, the variable, FOR; a statement map entry
 *
 * # asm structure (src/comp/comp.asm)
 * | asm | here | |
 * |---|---|---|
 * | cm_line, cm_text | comp_line, comp_text | MIRROR; D1 |
 * | cm_err | comp_fail | MIRROR; noreturn |
 * | cm_expn, cm_expect, cm_dfa | comp_expnames, comp_expected, comp_dfa | MIRROR |
 * | cm_toi, cm_tof, cm_tod, cm_tos, cm_tob, cm_toa, cm_dnz | comp_toint, comp_tofloat, comp_todouble, comp_tostring, comp_tobyte, comp_toaddr, comp_dtest | MIRROR |
 * | cm_byt, cm_op, cm_opx, cm_opw, cm_dw, cm_ins, cm_lint, cm_call, cm_ileaf, cm_shv | code_byte, code_op, code_opb, code_opw, code_dword, code_insert, code_int, code_call, code_intleaf, code_shclass (code.c) | MOVED |
 * | cm_icmp, cm_fcmp, cm_lflt | cg_icmp, cg_fcmp, cg_lfloat (codegen.c) | MOVED |
 * | (by kind) | comp_toinfo: a variable's kind; code_pick: one of four by T_* | D2 |
 * | cm_opid | comp_opid | MIRROR |
 * | cm_tgt, cm_store, cm_fst | comp_target, comp_store | MIRROR; A_FST's store too |
 * | cm_chkcl, cm_chkop, cm_chkcm, cm_chkeq, cm_chk | comp_close, comp_open, comp_comma, comp_equals, comp_need | MIRROR |
 * | cm_optcm, cm_lnum, cm_name, cm_jw | comp_optcomma, comp_lnum, comp_name, comp_lref | MIRROR |
 * | st_get, st_getn, st_eos, st_eosn | comp_peek, comp_next, comp_eos, eosnext | MIRROR |
 * | var_id, var_ispar, var_par, var_scope | comp_var, comp_ispar, comp_param, comp_scope | MIRROR |
 * | et_sfx, lbl_at, lbl_nam | comp_suffix, comp_label, comp_nameof | MIRROR |
 * | cm_rts | | gone: a return |
 * | cm_if, cm_fv, cm_fork, cm_for, cm_for1, cm_nv, cm_on, cm_onl, cm_print, cm_list, cm_dv, cm_dat, cm_rem, cm_in, cm_rd, cm_get, cm_run, cm_res, cm_write, cm_lin | act_if, act_fv, act_fork, act_for, act_for1, act_for2, act_nv, act_on, act_onl, act_print, act_list, act_dv, act_data, act_rem, act_in, act_rd, act_get, act_run, act_res, act_write, act_lin | MIRROR; D3 |
 * | cm_n0, cm_n1, cm_narg, cm_fop, cm_goto, cm_gosub, cm_ret | step_n0, step_n1, step_narg, step_fop, step_goto, step_gosub, step_ret | MIRROR |
 * | g_cmact | actions[] | MIRROR; `[A_x - A_BASE] = ...` |
 * | cm_new, cm_ee, cm_nx, cm_om, cm_fin, cm_mark, cm_dbg, cm_dbgx, cm_mapstmt, cm_tok, ps_look, cm_lnld, var_decl, cm_dim1, cm_dvas, cm_dsubs, cm_using | stmts (cm_new, cm_ee), ended (cm_nx, cm_om), finish, mark, dbg, dbgdrop, mapstmt, token, pslook, lnumload, declared, dim1, dimas, dimsubs, using | MIRROR |
 * | cm_do | token's dispatch | MIRROR |
 * | cm_end, cm_w, cm_skip | done and the end of comp_text, in code_opw, comp_skipdata | MIRROR |
 * | g_cbuf (g_cbufh before it) | comp.cbuf | D4 |
 * | g_xbuf | comp.xbuf[XLIM + COMP_TAIL] | MIRROR |
 * | g_etk | comp_etkind[7] | MIRROR |
 * | g_dlprc, g_lmod | comp.dlproc, comp.modlabel | MIRROR |
 * | g_psexp, g_psor, g_pscm, g_dmask | statics; comp.dmask | MIRROR |
 * | g_state.cm (TCOMP) | Comp; each field's comment names its asm field | D5 |
 * | (new) | comp_init: carves comp.cbuf (allo_carve); main.c calls it | |
 * | (new) | comp_cur, comp_curn, comp_end: st_get, st_getn, st_eos at comp.p | |
 * | (new) | comp_rd16, comp_rd32: load16, load32 (base.h), unaligned reads | |
 *
 * - D1. comp_text takes the body; the asm's SI is before it.
 * - D2. The asm picks by kind inline.
 * - D3. The actions are static, in one table.
 * - D4. comp.cbuf is an arena block with its byte before it.
 * - D5. ctxlin is gone: a literal is addr_of its text. cst is a local of act_list. csp is busy and
 *   unwind (a jmp_buf). rn.cplim is depth, cm_p's stack check as a nesting count (COMP_MAXDEPTH,
 *   to calibrate). wv is lnum, and comp_opw's word.
 */
#include "comp/comp.h"
#include "comp/blocks.h"
#include "comp/control.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "comp/expr.h"
#include "comp/refs.h"
#include "comp/codegen.h"
#include "comp/fold.h"
#include "comp/sema.h"
#include "io/console.h"
#include "io/msg.h"
#include "io/repl.h"
#include "mem/alloc.h"
#include "mem/scope.h"
#include "mem/symbols.h"
#include "num/fpmath.h"
#include "vm/debug.h"

const uint8_t comp_etkind[7] = {0, 1, 1, 0, SK_DBL, 2, 1};

bool comp_end(Comp *const C) { return comp_eos(C, &C->p); }

void comp_init(Comp *const C)
{
    C->cbuf = at(allo_carve(C->allo, 1 + LMAX + 64)) +
              1; /* the byte before: lexr's line before the body */
    C->code->buf = C->xbuf;
    C->dfa = 0xFFFF;
    ast_init(C->ast, C->fault);
}

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

/* ---- steps: the statement's nodes, checked as made, generated at a flush */

void comp_check(Comp *const C, Node *const n)
{
    const uint8_t *const p = C->p;
    sema_tree(C, n);
    C->p = p;
}

void comp_step(Comp *const C, Node *const n)
{
    comp_check(C, n);
    ast_step(C->ast, n);
}

void comp_flush(Comp *const C)
{
    Node *const first = C->ast->steps; /* taken: an error in them flushes none twice */
    C->ast->steps = C->ast->last = nullptr;

    for (Node *n = first; n; n = n->next) {
        sema_resolve(C, n);
        fold_tree(C, n);

        if (n->kind == N_STORE && n->mode) { /* a CONST's: its value folded before the store */
            uint8_t *const at = C->code->out;
            cg_store_value(C->code, n);
            fold_const(C, at, n->id);
            cg_store_op(C->code, n);
        } else
            cg_tree(C->code, n);
    }
}

/* A statement's node of kind, op, the next step. */
static Node *node(Comp *const C, const NodeKind kind, const uint8_t op)
{
    Node *const n = ast_new(C->ast, kind);
    n->op = op;
    ast_step(C->ast, n);
    return n;
}

uint8_t comp_type(Comp *const C)
{
    if (!ast_isvalue(C->ast->last))
        synerr(C);

    return ast_type(C->ast->last);
}

void comp_value(Comp *const C, const uint8_t type)
{
    C->ast->last->val = true;
    C->ast->last->type = type;
}

void comp_call(Comp *const C, const uint8_t k) { node(C, N_KCALL, k); }
void comp_int(Comp *const C, const uint32_t n) { node(C, N_INT, 0)->num = n; }
Node *comp_op(Comp *const C, const uint8_t op) { return node(C, N_OP, op); }
void comp_byte(Node *const n, const uint8_t b) { n->lit[n->nargs++] = b; }

void comp_word(Node *const n, const uint16_t w)
{
    comp_byte(n, (uint8_t)w);
    comp_byte(n, (uint8_t)(w >> 8));
}

void comp_opid(Comp *const C, const uint8_t op, const uint16_t id) { node(C, N_OPID, op)->id = id; }

Node *comp_jump(Comp *const C, const uint8_t op)
{
    Node *const n = node(C, N_JUMP, op);
    n->id = C->lnum;
    n->num = C->jline ? C->jline : LREF_AT | C->lnumat;
    return n;
}

Node *comp_go(Comp *const C, const uint8_t op, const Jump j)
{
    Node *const n = node(C, N_GO, op);
    n->jump = j;
    return n;
}

void comp_cond(Comp *const C, const uint8_t op, const Jump j)
{
    if (comp_type(C) & T_STR)
        mismatch(C);

    node(C, N_COND, op)->jump = j;
}

/* The last step's value converted, cv; Type mismatch from a string where a number goes, a number
   where a string. */
static void conv(Comp *const C, const uint8_t cv)
{
    if (cv == CV_STR ? !(comp_type(C) & T_STR) : comp_type(C) & T_STR)
        mismatch(C);

    ast_conv(C->ast, cv, C->p);
}

void comp_toint(Comp *const C) { conv(C, CV_INT); }
void comp_tofloat(Comp *const C) { conv(C, CV_FLT); }
void comp_todouble(Comp *const C) { conv(C, CV_DBL); }
void comp_tobyte(Comp *const C) { conv(C, CV_BYTE); }
void comp_toaddr(Comp *const C) { conv(C, CV_ADDR); }
void comp_tostring(Comp *const C) { conv(C, CV_STR); }

void comp_toinfo(Comp *const C, const uint8_t info)
{
    if (info & SK_DBL)
        conv(C, CV_DBL);
    else if ((info & VK_MASK) == VK_INT)
        conv(C, CV_INT);
    else if ((info & VK_MASK) == VK_FLT)
        conv(C, CV_FLT);
    else
        conv(C, CV_STR);
}

void comp_target(Comp *const C, const uint16_t id)
{
    C->tgt = id;
    C->tgtinfo = symb_var(C->symb, id)->info;
    C->tgtsubs = 0;
}

/* The last step's value stored to the target. A CONST's is generated now: its value, folded, is
   the next item's. */
void comp_store(Comp *const C)
{
    if (C->tgtsubs == TGT_FIELD) {
        ctrl_recstore(C);
        return;
    }

    const uint8_t info = C->tgtinfo;
    const bool str = comp_type(C) & T_STR;

    if (info & SK_DBL || (info & VK_MASK) == VK_INT || (info & VK_MASK) == VK_FLT) {
        if (str)
            mismatch(C);
    } else if ((info & VK_MASK) == VK_STR) {
        if (!str)
            mismatch(C);
    } else
        synerr(C);

    const uint16_t id = C->tgt;
    const uint16_t fix = (info & (SK_DBL | VK_MASK)) == VK_STR
                             ? scop_fix(C->scop, symb_var(C->symb, id)->name, C->tgtsubs)
                             : 0;
    Node *const n = ast_wrap(C->ast, N_STORE);
    n->id = id;
    n->op = C->tgtsubs;
    n->fix = fix;
    n->mode = C->tok == TK_CONST && !C->tgtsubs;

    if (n->mode)
        comp_flush(C);
}

/* ---- the token reader */

uint8_t comp_peek(const uint8_t **const p)
{
    while (**p == ' ')
        (*p)++;

    return **p;
}

uint8_t comp_next(const uint8_t **const p)
{
    (*p)++;
    return comp_peek(p);
}

bool comp_eos(Comp *const C, const uint8_t **const p)
{
    const uint8_t c = comp_peek(p);

    if (!c || c == ':' || c == TK_REM)
        return true;

    return c == TK_ELSE && C->oneif;
}

/* st_eosn: comp_eos a byte on. */
static bool eosnext(Comp *const C)
{
    C->p++;
    return comp_end(C);
}

void comp_need(Comp *const C, const uint8_t b)
{
    if (comp_cur(C) != b)
        synerr(C);

    comp_curn(C);
}

void comp_equals(Comp *const C) { comp_need(C, TK_EQ); }

bool comp_optcomma(Comp *const C)
{
    if (comp_cur(C) != ',')
        return false;

    C->p++;
    return true;
}

/* Label key of DX's kind: jline, in the scope's own or, when modlabel (cleared), the module's. */
static void lfind(Comp *const C, const uint16_t key, const uint16_t kind)
{
    const uint16_t scope = C->modlabel ? 0 : scop_own(C->scop);
    C->modlabel = false;
    C->jline = scop_lfind(C->scop, C->symb, scope | kind, key);
}

uint8_t comp_lnum(Comp *const C)
{
    C->jline = 0;
    C->lnumat = (uint32_t)(C->p - C->body);
    const uint8_t c = comp_cur(C);

    if (c == M_VAR) {
        lfind(C, comp_nameof(C->p), 0);
        C->lnum = NONUM;
        C->p = comp_skipname(C->p);
        return comp_cur(C);
    }

    if (c != M_INT) {
        C->modlabel = false;
        C->lnum = 0;
        return c;
    }

    const uint32_t n = load32(C->p + 1);

    if (n >= LNUM_LIMIT)
        synerr(C);

    C->lnum = (uint16_t)n;
    C->p += INT_TOKEN_SIZE;

    if (n) /* 0: ON ERROR GOTO 0's, none */
        lfind(C, (uint16_t)n, SC_NUM);

    C->modlabel = false;
    return comp_cur(C);
}

/* cm_lnld: I.LD.I16 of the line number; a label's line as LABEL_LINE + its line. */
static void lnumload(Comp *const C)
{
    if (C->lnum == NONUM)
        comp_int(C, C->jline + LABEL_LINE);
    else
        comp_word(comp_op(C, O_I_LD_I16), C->lnum);
}

/* ---- names and variables */

uint8_t comp_suffix(const uint8_t c)
{
    switch (c) {
    case '!':
        return ET_R4;

    case '#':
        return ET_R8;

    case '%':
        return ET_I2;

    case '&':
        return ET_I4;

    case '$':
        return ET_SD;

    default:
        return 0;
    }
}

const uint8_t *comp_label(const uint8_t *const body)
{
    if (*body != M_VAR)
        return nullptr;

    const uint8_t *const colon = comp_skipname(body);
    return *colon == ':' ? colon : nullptr;
}

uint16_t comp_nameof(const uint8_t *const at) { return load16(at + at[1] + 2); }

const uint8_t *comp_skipdata(const uint8_t *p)
{
    bool quoted = false;

    for (; *p && (quoted || *p != ':'); p++)
        if (*p == '"')
            quoted = !quoted;

    return p;
}

bool comp_ispar(Comp *const C, const uint16_t id)
{
    if (symb_var(C->symb, id)->info & VI_ARR || !C->proc)
        return false;

    const Proc *const p = blok_proc(C->blok, C->proc);

    for (uint32_t i = 0; i < p->npar; i++)
        if (p->params[i].var == id)
            return true;

    return false;
}

bool comp_param(Comp *const C, const uint16_t name, const uint8_t array, uint16_t *const id)
{
    if (!C->proc)
        return false;

    const Proc *const p = blok_proc(C->blok, C->proc);

    for (uint32_t i = 0; i < p->npar; i++) {
        const uint16_t v = p->params[i].var;
        const Var *const var = symb_var(C->symb, v);

        if (var->name == name && !((var->info ^ array) & VI_ARR)) {
            *id = v;
            return true;
        }
    }

    return false;
}

void comp_scope(Comp *const C, const uint16_t name)
{
    C->isstatic = false;
    C->scope = C->proc;

    if (!C->proc)
        return;

    uint16_t scope;
    const Binding *b = nullptr;

    if (!(symb_name(C->symb, name)->flags & NM_LBL))
        b = scop_look(C->scop, name, &scope);
    if (!b || !scope) {
        C->scope = 0; /* the module's */
        return;
    }

    if (b->flags & BF_STAT || blok_proc(C->blok, C->proc)->flags & PF_STAT)
        C->isstatic = true; /* STATIC */
}

/* var_decl: under OPTION EXPLICIT, name declared, or a procedure's; else Variable not declared. */
static void declared(Comp *const C, const uint16_t name)
{
    if (!C->decl->explicit_ || symb_name(C->symb, name)->flags & (NM_SUB | NM_FUNC | NM_FN))
        return;
    if (!scop_known(C->scop, name))
        fault_error(C->fault, E_VND);
}

uint16_t comp_var(Comp *const C, const uint8_t shape, const uint8_t et)
{
    const uint8_t *const q = C->p;
    const uint8_t first = q[2];
    const uint8_t sfx = comp_suffix(comp_suffixof(q));
    const uint16_t name = comp_nameof(q);
    C->p = comp_skipname(q);
    uint8_t kind = shape > 1 || (!shape && *C->p == '(') ? VI_ARR : 0;
    uint8_t type = et;

    if (!type && sfx)
        type = scop_sfx(C->scop, name, sfx, kind != 0);
    else if (!type) {
        uint16_t id;

        if (comp_param(C, name, kind, &id)) /* no suffix: a parameter's own type */
            return id;

        type = scop_type(C->scop, name, kind != 0);

        if (!type)
            type = C->symb->deft[upcase(first) - 'A'];
    }

    declared(C, name);

    if (type >= ET_REC) { /* a record: ref_recarray's, recvar's kind */
        if (!kind)
            kind = VI_RECARR;
    } else
        kind |= comp_etkind[type];

    comp_scope(C, name);
    return symb_find_var(C->symb, name, type, kind, C->scope, C->isstatic);
}

uint16_t comp_name(Comp *const C)
{
    if (comp_cur(C) != M_VAR)
        synerr(C);

    return comp_var(C, 0, 0);
}

/* ---- the statement map, DBG */

static void mapstmt(Comp *const C, const uint32_t bodyoff)
{
    uint8_t *const e = C->map - (C->nstmt + 1) * sizeof(Stmt);

    if (e < C->code->out + sizeof(Stmt)) { /* no room: the code is full */
        C->code->lim = C->code->out;
        return;
    }

    *(Stmt *)e = (Stmt){.body = bodyoff,
                        .code = (uint32_t)(C->code->out - C->code->buf)}; /* packed: any address */
    C->code->lim = e - COMP_TAIL;
    C->nstmt++;
}

/* The statement at comp.p: its body offset, its code's place; the last statement's nodes freed, an
   error's too. */
static void mark(Comp *const C)
{
    comp_flush(C); /* an IF's before its THEN's statement */
    ast_reset(C->ast);
    mapstmt(C, (uint32_t)(C->p - C->body));
}

/* DBG before a program statement's code, when the debugger wants it; not SUB's, FUNCTION's. */
static void dbg(Comp *const C)
{
    C->dbg = nullptr;

    if (C->dbug->emit && C->line != DLINE && *C->p != TK_SUB && *C->p != TK_FUNCTION) {
        *C->code->out++ = O_DBG;
        C->dbg = C->code->out;
    }
}

/* After the statement: its DBG gone when it made no code (DECLARE, DEFINT, ...). */
static void dbgdrop(Comp *const C)
{
    if (C->dbg == C->code->out)
        C->code->out--;

    C->dbg = nullptr;
}

/* ---- the line */

/* cm_fin: the line's end, EOL. */
static void finish(Comp *const C)
{
    mapstmt(C, BOFF);
    *C->code->out++ = O_EOL;
}

/* REM: the compile is over. */
[[noreturn]]
static void done(Comp *const C)
{
    comp_flush(C);
    finish(C);
    longjmp(C->unwind, 1);
}

void comp_fail(Comp *const C, const uint8_t err)
{
    comp_flush(C);                   /* the steps checked before the error */
    uint8_t *const o = C->code->out; /* ERR err dg at:4 state:2 */
    o[0] = O_ERR;
    o[1] = err;
    o[2] = C->dg;
    store32(o + 3, (uint32_t)(C->p - C->body));
    store16(o + 7, C->dfa);
    C->code->out = o + 9;
    C->dg = 0;
    C->dfa = 0xFFFF;
    done(C);
}

static uint32_t pslook(const uint32_t st, const uint8_t cls)
{
    for (uint32_t i = lexr_ps_first[st]; i < lexr_ps_first[st + 1]; i++)
        if (lexr_ps_xclass[i] == cls)
            return lexr_ps_xentry[i];

    return lexr_ps_default[st];
}

static void (*const actions[A_END - A_BASE])(Comp *C);

void comp_dfa(Comp *const C, const uint16_t state)
{
    uint32_t st = state;

    for (;;) {
        const uint32_t e = pslook(st, lexr_ps_class[comp_cur(C)]);
        const uint8_t code = (uint8_t)e;

        if (code < PS_ACC) {
            st = e >> 8;

            if (code < A_BASE)
                comp_call(C, code);
            else
                actions[code - A_BASE](C);

            continue;
        }

        if (code == PS_ACC)
            return;

        if (code == PS_ERR) { /* for the error's words: what it wanted */
            C->dfa = (uint16_t)st;
            synerr(C);
        }

        st = e >> 8;

        if (code == PS_TOK)
            C->p++;
    }
}

/* State st's names to names[n..]; past FFh (follow) the keywords of the state it leads to. */
static uint8_t expnames(const uint32_t st, const bool follow, const char *names[], uint8_t n)
{
    for (uint8_t cls = 0; cls < PS_NC; cls++) {
        const uint32_t e = pslook(st, cls);

        if ((uint8_t)e < PS_EOS)
            continue;
        if (cls == lexr_ps_class[0xFF]) {
            if (follow)
                n = expnames(e >> 8, false, names, n);

            continue;
        }

        if (!follow && cls == lexr_ps_class[0]) /* past FFh the end's class is ACCESS's */
            continue;
        if (*lexr_ps_classname[cls])
            names[n++] = lexr_ps_classname[cls];
    }

    return n;
}

uint8_t comp_expnames(const uint16_t state, const char *names[])
{
    return expnames(state, true, names, 0);
}

void comp_expected(Cons *const co, const uint16_t state)
{
    const char *names[PS_NC * 2];
    const uint8_t n = comp_expnames(state, names);

    for (uint8_t i = 0; i < n; i++) {
        cons_puts(co, names[i]);
        const uint8_t left = n - i - 1;

        if (left)
            cons_puts(co, left == 1 ? " or " : ", ");
    }
}

/* cm_tok: the statement at comp.p. */
static void token(Comp *const C)
{
    C->tok = 0; /* an implied LET's: none */
    mark(C);
    dbg(C);

    if (ctrl_stmt(C))
        return;

    const uint8_t c = *C->p;

    if (c == M_VAR) {
        comp_dfa(C, ST_LET);
        return;
    }

    if (c < 0x80)
        synerr(C);

    C->tok = c;
    C->kw = C->p;
    uint16_t st;

    if (c == TK_X) { /* FFh n: its own table, past both */
        st = lexr_ps_entry1[C->p[1]];
        C->p++;
    } else
        st = lexr_ps_entry[c - 0x80];

    if (!st)
        synerr(C);

    comp_curn(C);
    comp_dfa(C, st);
}

/* cm_nx: the statement compiled, its end next. */
static void ended(Comp *const C)
{
    comp_flush(C);

    if (C->code->full) { /* cm_om: the code is full, its last op cut short: from its statement */
        C->code->out =
            C->code->buf + (C->nstmt ? ((const Stmt *)C->map)[-(int32_t)C->nstmt].code : 0);
        comp_fail(C, E_OM);
    }

    dbgdrop(C);

    if (!comp_end(C))
        synerr(C);
}

/* The statements after comp.p, to the line's end. */
static void stmts(Comp *const C)
{
    for (;;) { /* cm_new: comp.p before a statement */
        C->then = false;

        if (!eosnext(C)) {
            token(C);
            ended(C);
        }

        for (;;) { /* cm_ee: 0 or ' the line ends, ':' another statement, ELSE a one-line IF's */
            uint8_t c = *C->p;

            if (!c || c == TK_REM)
                return;
            if (c == ':')
                break;

            code_op(C->code, O_NXLN); /* ELSE: the THEN part leaves the line */
            c = comp_curn(C);

            if (!c)
                synerr(C);
            if (c == M_INT) {
                mark(C);
                comp_dfa(C, ST_GOTO); /* ELSE line */
            } else
                token(C);

            ended(C);
        }
    }
}

void comp_text(Comp *const C, const uint8_t *const body)
{
    C->busy = true;
    C->depth = 0;
    C->body = body;
    C->nstmt = 0;
    C->nextkw = nullptr;
    C->oneif = false;
    C->dg = 0;
    C->dfa = 0xFFFF;
    C->code->iload = C->code->iloadend = C->code->cmpend = nullptr;
    C->code->out = C->code->buf;
    C->code->full = false;
    C->map = C->code->buf + XLIM + COMP_TAIL; /* the map grows down from the top */
    C->code->lim = C->map - COMP_TAIL;
    const uint8_t *colon = comp_label(body); /* a label: on from its ':' */
    C->p = colon ? colon : body - 1;

    if (!setjmp(C->unwind)) {
        stmts(C);
        finish(C);
    }

    C->busy = false;
    C->len = (uint8_t)(C->code->out - C->code->buf);
}

void comp_proc(Comp *const C, const uint8_t proc)
{
    C->proc = proc;
    scop_enter(C->scop, proc ? blok_proc(C->blok, proc) : nullptr);
}

void comp_line(Comp *const C)
{
    C->line = DLINE;
    C->ndfor = 0;
    comp_proc(C, C->dlproc);
    decl_deftype(C->decl, C->symb, 0xFFFFFFFF); /* the program's end's DEFtypes */
    C->code->buf = C->xbuf;
    comp_text(C, C->cbuf);
}

/* ---- the actions: each makes the statement's steps */

/* The one of four for the last step's value's type. */
static uint8_t by_type(
    Comp *const C,
    const uint8_t i,
    const uint8_t f,
    const uint8_t d,
    const uint8_t s
)
{
    return code_pick(comp_type(C), i, f, d, s);
}

/* A_E: an expression. */
static void act_expr(Comp *const C) { comp_step(C, expr_tree(C, false)); }

/* A_LE: the value of a target, a DOUBLE's parsed as such. */
static void act_assign(Comp *const C) { comp_step(C, expr_tree(C, C->tgtinfo & SK_DBL)); }

/* IF expr THEN|GOTO line or statements: false past the ELSE, else the one-line IF's test. */
static void act_if(Comp *const C)
{
    expr_compile(C);
    const uint8_t type = comp_type(C);
    node(C, N_TEST, 0); /* a DOUBLE's D.NZ: before the THEN's check, as an error's code has it */
    comp_value(C, type == T_DBL ? T_INT : type);
    uint8_t c = *C->p;

    if (c != TK_THEN && c != TK_GOTO)
        synerr(C);
    if (c == TK_THEN && (!C->p[1] || C->p[1] == TK_REM)) { /* a block IF */
        ctrl_blockif(C);
        return;
    }

    C->oneif = true;
    uint32_t t;

    if (ctrl_else(C, &t))
        ctrl_jf(C, t); /* false: past the ELSE */
    else {
        comp_type(C);
        node(C, N_IF, 0);
    }

    C->then = true;
    const uint8_t kw = *C->p; /* THEN or GOTO */
    c = comp_curn(C);

    if (kw == TK_GOTO || c == M_INT)
        comp_dfa(C, ST_GOTO);
    else
        token(C);
}

/* FOR's variable: the target of the start's store. */
static void act_fv(Comp *const C)
{
    const uint16_t id = comp_name(C);
    const uint8_t info = symb_var(C->symb, id)->info;

    if (info & VI_ARR)
        synerr(C);

    C->forvar = id;
    comp_target(C, id);

    if (info & SK_DBL) /* a DOUBLE's FOR: not yet */
        mismatch(C);

    const uint8_t kind = info & VK_MASK;

    if (kind == VK_STR)
        mismatch(C);

    C->forkind = kind;
}

/* The value as FOR's variable wants it. */
static void act_fork(Comp *const C) { comp_toinfo(C, C->forkind); }

static void act_for(Comp *const C, const uint8_t op)
{
    ctrl_for(C, C->forkind ? op : op + 1); /* F.FOR, F.FOR1 after the INT's */
}
static void act_for2(Comp *const C) { act_for(C, O_I_FOR); }
static void act_for1(Comp *const C) { act_for(C, O_I_FOR1); }

/* NEXT [v]: one of the loops it closes. */
static void act_nv(Comp *const C) { ctrl_next(C); }

/* DATA: no code, READ finds the text in the body. */
static void act_data(Comp *const C) { C->p = comp_skipdata(C->kw + 1); }

/* REM: the line's end. */
static void act_rem(Comp *const C) { done(C); }

/* RUN, RESTORE [line]: CALL, or the line and CALL .L. */
static void runres(Comp *const C, const uint8_t k)
{
    if (comp_end(C)) {
        comp_call(C, k);
        return;
    }

    C->modlabel = true; /* the module's label */
    comp_lnum(C);
    lnumload(C);
    comp_call(C, k + 1);
}
static void act_run(Comp *const C) { runres(C, K_RUN); }
static void act_res(Comp *const C) { runres(C, K_RES); }

/* DIM's ( [lo TO] hi {, ..} ): lo hi, or hi and OPTION BASE's, each an INT; how many. */
static uint32_t dimsubs(Comp *const C)
{
    comp_open(C);
    uint32_t n = 0;
    C->dmask = 0;

    do {
        expr_compile(C);
        comp_toint(C);

        if (comp_cur(C) == TK_TO) {
            C->p++;
            expr_compile(C);
            comp_toint(C);
        } else {
            comp_int(C, C->decl->base);
            C->dmask |= (uint64_t)1 << (n & 63);
        }

        n++;
    } while (comp_optcomma(C));

    comp_close(C);
    return n;
}

/* DIM's array id: ( subscripts ), the mask as 2 INTs, its id, the count, CALL DIM. A STATIC's
   array is dimensioned once. */
static void dim1(Comp *const C, const uint16_t id)
{
    const uint32_t flags = (C->isstatic ? DIM_STATIC : 0) | C->redim;
    const uint32_t n = dimsubs(C);
    comp_int(C, (uint32_t)C->dmask);
    comp_int(C, (uint32_t)(C->dmask >> 32));
    comp_int(C, id);
    comp_int(C, n | flags);
    comp_call(C, K_DIM);
}

/* Past DIM's AS type [* n]: decl_line's. */
static void dimas(Comp *const C)
{
    const uint8_t *p = C->p;

    if (*p != TK_AS)
        return;
    if (p[1] == M_VAR) { /* AS a TYPE */
        C->p = decl_item(p + 1);
        return;
    }

    if (p[1] == TK_X) { /* AS SHORT: AS, FFh n */
        C->p = p + 3;
        return;
    }

    C->p = p + 2;

    if (p[1] == TK_STRING && p[2] == TK_MUL) /* STRING * n */
        C->p += 1 + INT_TOKEN_SIZE;
}

/* A DIM item: the subscripts, its id, n, CALL DIM; a scalar STRING * n: n spaces. */
static void act_dv(Comp *const C)
{
    uint16_t id = comp_name(C);

    if (*C->p == '(') {
        const uint16_t name = symb_var(C->symb, id)->name;
        const uint8_t t = scop_rec(C->scop, name, true); /* a record array: its own variable */
        if (t)
            id = ref_recarray(C, name, t);

        dim1(C, id);
        dimas(C);
        return;
    }

    dimas(C);
    const uint16_t fix = scop_fix(C->scop, symb_var(C->symb, id)->name, false);

    if (!fix || symb_var(C->symb, id)->info & VI_ARR)
        return;

    comp_int(C, fix);
    comp_call(C, K_SPACE);
    comp_opid(C, O_S_ST_V, id);
}

/* INPUT's, READ's, GET's value for the target, stored. */
static void readto(Comp *const C, uint8_t k)
{
    uint8_t type = T_FLT;
    const uint8_t info = C->tgtinfo;

    if (info & SK_DBL && k != K_FGET) {
        k = k == K_FIN ? K_DIN : K_DREAD;
        type = T_DBL;
    } else if ((info & VK_MASK) == VK_STR) {
        k++;
        type = T_STR;
    }

    comp_call(C, k);
    comp_value(C, type);
    comp_store(C);
}
static void act_in(Comp *const C) { readto(C, K_FIN); }
static void act_rd(Comp *const C) { readto(C, K_FREAD); }
static void act_get(Comp *const C) { readto(C, K_FGET); }

/* PRINT USING's items: each by its type; US.END, or US.ENDN when ; or , ends the list. */
static void using(Comp *const C)
{
    for (;;) {
        if (comp_end(C))
            break;

        expr_compile(C);
        comp_call(C, by_type(C, K_USI, K_USF, K_USD, K_USS));

        if (comp_end(C))
            break;

        const uint8_t c = *C->p;

        if (c != ',' && c != ';')
            synerr(C);
        if (eosnext(C)) {
            comp_call(C, K_USEN);
            return;
        }
    }

    comp_call(C, K_USE);
}

/* PRINT items; PR.NL unless the list ends with ; , TAB( or SPC(. USING: using's items. */
static void act_print(Comp *const C)
{
    if (comp_cur(C) == TK_X && C->p[1] == TK1_USING) {
        C->p += 2; /* USING format; items */
        expr_compile(C);
        comp_tostring(C);
        comp_call(C, K_USB);

        if (comp_cur(C) != ';')
            synerr(C);

        C->p++;
        using(C);
        return;
    }

    if (comp_end(C)) {
        comp_call(C, K_PNL);
        return;
    }

    for (;;) {
        const uint8_t c = *C->p;

        if (c == ',')
            comp_call(C, K_PZONE);
        else if (c == TK_TAB || c == TK_SPC) {
            C->p++;
            expr_paren(C);
            comp_tobyte(C);
            C->p--;
            comp_call(C, c == TK_TAB ? K_PTAB : K_PSPC);
        } else if (c != ';') {
            expr_compile(C);
            comp_call(C, by_type(C, K_IPR, K_FPR, K_DPR, K_SPR));

            if (comp_end(C)) {
                comp_call(C, K_PNL);
                return;
            }

            continue;
        }

        if (eosnext(C))
            return;
    }
}

/* WRITE items: each by its type, a comma between, PR.NL after. */
static void act_write(Comp *const C)
{
    for (;;) {
        if (comp_end(C))
            break;

        expr_compile(C);
        comp_call(C, by_type(C, K_WI, K_WF, K_WD, K_WS));

        if (comp_end(C))
            break;

        const uint8_t c = *C->p;

        if (c != ',' && c != ';')
            synerr(C);

        C->p++;
        comp_call(C, K_WCOM);
    }

    comp_call(C, K_PNL);
}

/* LINE INPUT's target: the line, stored. */
static void act_lin(Comp *const C)
{
    comp_call(C, K_SLINE);
    comp_value(C, T_STR);
    comp_store(C);
}

/* LIST [from][-[to]]: the two numbers, CALL LIST. */
static void act_list(Comp *const C)
{
    const uint8_t *const start = C->p;
    const uint8_t c = comp_lnum(C);
    lnumload(C);

    if (c == TK_MINUS) {
        if (eosnext(C))
            C->lnum = 0xFFFF;
        else
            comp_lnum(C);
    } else if (C->p == start)
        C->lnum = 0xFFFF;

    lnumload(C);
    comp_call(C, K_LIST);
}

/* GOTO|GOSUB after ON's expression: ON kind + n, its lines after. */
static void act_on(Comp *const C)
{
    comp_tobyte(C);
    const uint8_t c = *C->p;

    if (c != TK_GOTO && c != TK_GOSUB) {
        C->dg = DG_GOTO_MISSING;
        synerr(C);
    }

    C->on = node(C, N_ON, c == TK_GOSUB ? ON_GOSUB : 0);
    C->onlast = nullptr;
    C->p++;
}

/* One of ON's lines: num addr. */
static void act_onl(Comp *const C)
{
    comp_lnum(C);
    Node *const r = ast_new(C->ast, N_JUMP);
    r->id = C->lnum;
    r->num = C->jline ? C->jline : LREF_AT | C->lnumat;
    ast_link(C->on, &C->onlast, r);
}

/* A_FOP: CALL LOAD, SAVE or VERIFY by the token. */
static void act_fop(Comp *const C) { comp_call(C, C->tok + K_LOAD - TK_LOAD); }

/* A_N0, A_N1: the arguments counted; no code. */
static void act_n0(Comp *const C) { C->nargs = 0; }
static void act_n1(Comp *const C) { C->nargs++; }

/* A_NARG: their count. */
static void act_narg(Comp *const C) { comp_int(C, C->nargs); }

/* A_L: a line number or label read; no code. */
static void act_lnum(Comp *const C) { comp_lnum(C); }

static void act_goto(Comp *const C) { comp_jump(C, O_GOTO); }
static void act_gosub(Comp *const C) { comp_jump(C, O_GOSUB); }
static void act_ret(Comp *const C) { comp_op(C, O_RETURN); }

static void (*const actions[A_END - A_BASE])(Comp *C) = {
    [A_E - A_BASE] = act_expr,        [A_LV - A_BASE] = expr_target,
    [A_LE - A_BASE] = act_assign,     [A_LET - A_BASE] = comp_store,
    [A_FST - A_BASE] = comp_store,    [A_TOI - A_BASE] = comp_toint,
    [A_TOD - A_BASE] = comp_todouble, [A_TOB - A_BASE] = comp_tobyte,
    [A_TOA - A_BASE] = comp_toaddr,   [A_TOS - A_BASE] = comp_tostring,
    [A_STR - A_BASE] = expr_string,   [A_VA - A_BASE] = expr_varaddr,
    [A_L - A_BASE] = act_lnum,        [A_FV - A_BASE] = act_fv,
    [A_FK - A_BASE] = act_fork,       [A_FOR - A_BASE] = act_for2,
    [A_FOR1 - A_BASE] = act_for1,     [A_NV - A_BASE] = act_nv,
    [A_IF - A_BASE] = act_if,         [A_ON - A_BASE] = act_on,
    [A_ONL - A_BASE] = act_onl,       [A_PR - A_BASE] = act_print,
    [A_LST - A_BASE] = act_list,      [A_WR - A_BASE] = act_write,
    [A_LIN - A_BASE] = act_lin,       [A_DV - A_BASE] = act_dv,
    [A_DATA - A_BASE] = act_data,     [A_REM - A_BASE] = act_rem,
    [A_IN - A_BASE] = act_in,         [A_RD - A_BASE] = act_rd,
    [A_GET - A_BASE] = act_get,       [A_RUN - A_BASE] = act_run,
    [A_RES - A_BASE] = act_res,       [A_SWAP - A_BASE] = ctrl_swap,
    [A_FOP - A_BASE] = act_fop,       [A_N0 - A_BASE] = act_n0,
    [A_N1 - A_BASE] = act_n1,         [A_NARG - A_BASE] = act_narg,
    [A_GOTO - A_BASE] = act_goto,     [A_GOSUB - A_BASE] = act_gosub,
    [A_RET - A_BASE] = act_ret,
};
