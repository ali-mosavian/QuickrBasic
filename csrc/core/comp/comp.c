/* The compiler: a token body to stack-machine code, one statement at a time.
 *
 * # How it works
 * A statement is run by the generated automaton (comp/dfa.h): each state's entry for the byte at
 * the cursor steps past a token, goes on at the statement's end, or runs an action, an emitter
 * (A_*) or CALL K_*. The token cursor is comp.p, the code's next byte comp.out. The helpers here also take
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
 * | (by kind) | comp_toinfo: a variable's kind; comp_pick: one of four by T_* | D2 |
 * | cm_opid | comp_opid | MIRROR |
 * | cm_tgt, cm_store, cm_fst | comp_target, comp_store | MIRROR; A_FST's store too |
 * | cm_chkcl, cm_chkop, cm_chkcm, cm_chkeq, cm_chk | comp_close, comp_open, comp_comma, comp_equals, comp_need | MIRROR |
 * | cm_optcm, cm_lnum, cm_name, cm_jw | comp_optcomma, comp_lnum, comp_name, comp_lref | MIRROR |
 * | st_get, st_getn, st_eos, st_eosn | comp_peek, comp_next, comp_eos, eosnext | MIRROR |
 * | var_id, var_ispar, var_par, var_scope | comp_var, comp_ispar, comp_param, comp_scope | MIRROR |
 * | et_sfx, lbl_at, lbl_nam | comp_suffix, comp_label, comp_nameof | MIRROR |
 * | cm_rts | | gone: a return |
 * | cm_if, cm_fv, cm_fork, cm_for, cm_for1, cm_nv, cm_on, cm_onl, cm_print, cm_list, cm_dv, cm_dat, cm_rem, cm_in, cm_rd, cm_get, cm_goto, cm_gosub, cm_run, cm_res, cm_ret, cm_n0, cm_n1, cm_narg, cm_fop, cm_write, cm_lin | act_if, act_fv, act_fork, act_for, act_for1, act_for2, act_nv, act_on, act_onl, act_print, act_list, act_dv, act_data, act_rem, act_in, act_rd, act_get, act_goto, act_gosub, act_run, act_res, act_ret, act_n0, act_n1, act_narg, act_fop, act_write, act_lin | MIRROR; D3 |
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
#include "session.h"
#include "comp/blocks.h"
#include "comp/control.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "comp/expr.h"
#include "comp/refs.h"
#include "comp/fold.h"
#include "io/console.h"
#include "io/msg.h"
#include "io/repl.h"
#include "mem/alloc.h"
#include "mem/scope.h"
#include "mem/symbols.h"
#include "num/fpmath.h"
#include "vm/debug.h"

const uint8_t comp_etkind[7] = {0, 1, 1, 0, SK_DBL, 2, 1};

bool comp_end(Session *const S) { return comp_eos(S, &S->comp.p); }

void comp_init(Session *const S)
{
    S->comp.cbuf =
        at(allo_carve(S, 1 + LMAX + 64)) + 1; /* the byte before: lexr's line before the body */
    S->code.buf = S->comp.xbuf;
    S->comp.dfa = 0xFFFF;
    ast_init(S);
}

[[noreturn]]
static void synerr(Session *const S)
{
    repl_error(S, E_SN);
}
[[noreturn]]
static void mismatch(Session *const S)
{
    repl_error(S, E_TM);
}

/* ---- emitting: the statements' own, their checks with them */

void comp_lref(Session *const S)
{
    code_word(&S->code, S->comp.lnum);
    code_dword(&S->code, S->comp.jline);
}

void comp_opid(Session *const S, const uint8_t op, const uint16_t id)
{
    code_opid(&S->code, op, id, comp_ispar(S, id));
}

/* The value a number; else Type mismatch. */
static void number(Session *const S)
{
    if (S->code.type & T_STR)
        mismatch(S);
}

void comp_toint(Session *const S)
{
    number(S);
    code_toint(&S->code);
}
void comp_tofloat(Session *const S)
{
    number(S);
    code_tofloat(&S->code);
}
void comp_todouble(Session *const S)
{
    number(S);
    code_todouble(&S->code);
}
void comp_tobyte(Session *const S)
{
    number(S);
    code_tobyte(&S->code);
}
void comp_toaddr(Session *const S)
{
    number(S);
    code_toaddr(&S->code);
}

void comp_tostring(Session *const S)
{
    if (!(S->code.type & T_STR))
        mismatch(S);
}

void comp_toinfo(Session *const S, const uint8_t info)
{
    if ((info & (SK_DBL | VK_MASK)) == VK_STR)
        comp_tostring(S);
    else
        number(S);

    code_toinfo(&S->code, info);
}

void comp_target(Session *const S, const uint16_t id)
{
    S->comp.tgt = id;
    S->comp.tgtinfo = symb_var(&S->symb, id)->info;
    S->comp.tgtsubs = 0;
}

/* The target's store by its subscripts: none, one, or many and their count. */
static void store_to(Session *const S, const uint8_t scalar, const uint8_t one, const uint8_t many)
{
    const uint8_t subs = S->comp.tgtsubs;

    if (subs > 1) {
        comp_opid(S, many, S->comp.tgt);
        code_byte(&S->code, subs);
    } else
        comp_opid(S, subs ? one : scalar, S->comp.tgt);
}

/* The value converted for the target; its store ops' offset from the INT's: 0, 1 FLOAT, 2 STRING. */
static uint8_t convert_for_target(Session *const S)
{
    switch (S->comp.tgtinfo & VK_MASK) {
    case VK_INT: /* a SHORT: Overflow past 16 bits, unless it is one */
        code_shclass(&S->code);
        comp_toint(S);

        if (symb_var(&S->symb, S->comp.tgt)->type == ET_SH && !S->code.shclass)
            code_op(&S->code, O_I_CHK16);

        return 0;

    case VK_FLT:
        comp_tofloat(S);
        return 1;

    case VK_STR: { /* a STRING * n: n chars */
        comp_tostring(S);
        const uint16_t fix =
            scop_fix(&S->scop, symb_var(&S->symb, S->comp.tgt)->name, S->comp.tgtsubs != 0);

        if (fix) {
            code_int(&S->code, fix);
            code_call(&S->code, K_SFIX);
        }

        return 2;
    }
    default:
        synerr(S);
    }
}

void comp_store(Session *const S)
{
    if (S->comp.tgtsubs == TGT_FIELD) {
        ctrl_recstore(S);
        return;
    }

    if (S->comp.tgtinfo & SK_DBL) {
        comp_todouble(S);
        fold_const(S);
        store_to(S, O_D_ST_V, O_D_ST_A1, O_D_ST_A);
        return;
    }

    const uint8_t k = convert_for_target(S);
    fold_const(S);
    store_to(S, O_I_ST_V + k, O_I_ST_A1 + k, O_I_ST_A + k);
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

bool comp_eos(Session *const S, const uint8_t **const p)
{
    const uint8_t c = comp_peek(p);

    if (!c || c == ':' || c == TK_REM)
        return true;

    return c == TK_ELSE && S->comp.oneif;
}

/* st_eosn: comp_eos a byte on. */
static bool eosnext(Session *const S)
{
    S->comp.p++;
    return comp_end(S);
}

void comp_need(Session *const S, const uint8_t b)
{
    if (comp_cur(&S->comp) != b)
        synerr(S);

    comp_curn(&S->comp);
}

void comp_equals(Session *const S) { comp_need(S, TK_EQ); }

bool comp_optcomma(Session *const S)
{
    if (comp_cur(&S->comp) != ',')
        return false;

    S->comp.p++;
    return true;
}

/* Label key of DX's kind: jline, in the scope's own or, when modlabel (cleared), the module's. */
static void lfind(Session *const S, const uint16_t key, const uint16_t kind)
{
    const uint16_t scope = S->comp.modlabel ? 0 : scop_own(&S->scop);
    S->comp.modlabel = false;
    S->comp.jline = scop_lfind(&S->scop, &S->symb, scope | kind, key);
}

uint8_t comp_lnum(Session *const S)
{
    S->comp.jline = 0;
    const uint8_t c = comp_cur(&S->comp);

    if (c == M_VAR) {
        lfind(S, comp_nameof(S->comp.p), 0);
        S->comp.lnum = NONUM;
        S->comp.p = comp_skipname(S->comp.p);
        return comp_cur(&S->comp);
    }

    if (c != M_INT) {
        S->comp.modlabel = false;
        S->comp.lnum = 0;
        return c;
    }

    const uint32_t n = load32(S->comp.p + 1);

    if (n >= LNUM_LIMIT)
        synerr(S);

    S->comp.lnum = (uint16_t)n;
    S->comp.p += INT_TOKEN_SIZE;

    if (n) /* 0: ON ERROR GOTO 0's, none */
        lfind(S, (uint16_t)n, SC_NUM);

    S->comp.modlabel = false;
    return comp_cur(&S->comp);
}

/* cm_lnld: I.LD.I16 of the line number; a label's line as LABEL_LINE + its line. */
static void lnumload(Session *const S)
{
    if (S->comp.lnum == NONUM)
        code_int(&S->code, S->comp.jline + LABEL_LINE);
    else
        code_opw(&S->code, O_I_LD_I16, S->comp.lnum);
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

bool comp_ispar(Session *const S, const uint16_t id)
{
    if (symb_var(&S->symb, id)->info & VI_ARR || !S->comp.proc)
        return false;

    const Proc *const p = blok_proc(&S->blok, S->comp.proc);

    for (uint32_t i = 0; i < p->npar; i++)
        if (p->params[i].var == id)
            return true;

    return false;
}

bool comp_param(Session *const S, const uint16_t name, const uint8_t array, uint16_t *const id)
{
    if (!S->comp.proc)
        return false;

    const Proc *const p = blok_proc(&S->blok, S->comp.proc);

    for (uint32_t i = 0; i < p->npar; i++) {
        const uint16_t v = p->params[i].var;
        const Var *const var = symb_var(&S->symb, v);

        if (var->name == name && !((var->info ^ array) & VI_ARR)) {
            *id = v;
            return true;
        }
    }

    return false;
}

void comp_scope(Session *const S, const uint16_t name)
{
    S->comp.isstatic = false;
    S->comp.scope = S->comp.proc;

    if (!S->comp.proc)
        return;

    uint16_t scope;
    const Binding *b = nullptr;

    if (!(symb_name(&S->symb, name)->flags & NM_LBL))
        b = scop_look(&S->scop, name, &scope);
    if (!b || !scope) {
        S->comp.scope = 0; /* the module's */
        return;
    }

    if (b->flags & BF_STAT || blok_proc(&S->blok, S->comp.proc)->flags & PF_STAT)
        S->comp.isstatic = true; /* STATIC */
}

/* var_decl: under OPTION EXPLICIT, name declared, or a procedure's; else Variable not declared. */
static void declared(Session *const S, const uint16_t name)
{
    if (!S->decl.explicit_ || symb_name(&S->symb, name)->flags & (NM_SUB | NM_FUNC | NM_FN))
        return;
    if (!scop_known(&S->scop, name))
        repl_error(S, E_VND);
}

uint16_t comp_var(Session *const S, const uint8_t shape, const uint8_t et)
{
    const uint8_t *const q = S->comp.p;
    const uint8_t first = q[2];
    const uint8_t sfx = comp_suffix(comp_suffixof(q));
    const uint16_t name = comp_nameof(q);
    S->comp.p = comp_skipname(q);
    uint8_t kind = shape > 1 || (!shape && *S->comp.p == '(') ? VI_ARR : 0;
    uint8_t type = et;

    if (!type && sfx)
        type = scop_sfx(&S->scop, name, sfx, kind != 0);
    else if (!type) {
        uint16_t id;

        if (comp_param(S, name, kind, &id)) /* no suffix: a parameter's own type */
            return id;

        type = scop_type(&S->scop, name, kind != 0);

        if (!type)
            type = S->symb.deft[upcase(first) - 'A'];
    }

    declared(S, name);

    if (type >= ET_REC) { /* a record: ref_recarray's, recvar's kind */
        if (!kind)
            kind = VI_RECARR;
    } else
        kind |= comp_etkind[type];

    comp_scope(S, name);
    return symb_find_var(S, name, type, kind, S->comp.scope, S->comp.isstatic);
}

uint16_t comp_name(Session *const S)
{
    if (comp_cur(&S->comp) != M_VAR)
        synerr(S);

    return comp_var(S, 0, 0);
}

/* ---- the statement map, DBG */

static void mapstmt(Session *const S, const uint32_t bodyoff)
{
    uint8_t *const e = S->comp.map - (S->comp.nstmt + 1) * sizeof(Stmt);

    if (e < S->code.out + sizeof(Stmt)) { /* no room: the code is full */
        S->code.lim = S->code.out;
        return;
    }

    *(Stmt *)e = (Stmt){.body = bodyoff,
                        .code = (uint32_t)(S->code.out - S->code.buf)}; /* packed: any address */
    S->code.lim = e - COMP_TAIL;
    S->comp.nstmt++;
}

/* The statement at comp.p: its body offset, its code's place; the last statement's nodes freed, an
   error's too. */
static void mark(Session *const S)
{
    ast_reset(S);
    mapstmt(S, (uint32_t)(S->comp.p - S->comp.body));
}

/* DBG before a program statement's code, when the debugger wants it; not SUB's, FUNCTION's. */
static void dbg(Session *const S)
{
    S->comp.dbg = nullptr;

    if (S->dbug.emit && S->comp.line != DLINE && *S->comp.p != TK_SUB &&
        *S->comp.p != TK_FUNCTION) {
        *S->code.out++ = O_DBG;
        S->comp.dbg = S->code.out;
    }
}

/* After the statement: its DBG gone when it made no code (DECLARE, DEFINT, ...). */
static void dbgdrop(Session *const S)
{
    if (S->comp.dbg == S->code.out)
        S->code.out--;

    S->comp.dbg = nullptr;
}

/* ---- the line */

/* cm_fin: the line's end, EOL. */
static void finish(Session *const S)
{
    mapstmt(S, BOFF);
    *S->code.out++ = O_EOL;
}

/* REM: the compile is over. */
[[noreturn]]
static void done(Session *const S)
{
    finish(S);
    longjmp(S->comp.unwind, 1);
}

void comp_fail(Session *const S, const uint8_t err)
{
    uint8_t *const o = S->code.out; /* ERR err dg at:4 state:2 */
    o[0] = O_ERR;
    o[1] = err;
    o[2] = S->comp.dg;
    store32(o + 3, (uint32_t)(S->comp.p - S->comp.body));
    store16(o + 7, S->comp.dfa);
    S->code.out = o + 9;
    S->comp.dg = 0;
    S->comp.dfa = 0xFFFF;
    done(S);
}

static uint32_t pslook(const uint32_t st, const uint8_t cls)
{
    for (uint32_t i = lexr_ps_first[st]; i < lexr_ps_first[st + 1]; i++)
        if (lexr_ps_xclass[i] == cls)
            return lexr_ps_xentry[i];

    return lexr_ps_default[st];
}

static void (*const actions[A_END - A_BASE])(Session *S);

void comp_dfa(Session *const S, const uint16_t state)
{
    uint32_t st = state;

    for (;;) {
        const uint32_t e = pslook(st, lexr_ps_class[comp_cur(&S->comp)]);
        const uint8_t code = (uint8_t)e;

        if (code < PS_ACC) {
            st = e >> 8;

            if (code < A_BASE)
                code_call(&S->code, code);
            else
                actions[code - A_BASE](S);

            continue;
        }

        if (code == PS_ACC)
            return;
        if (code == PS_ERR) { /* for the error's words: what it wanted */
            S->comp.dfa = (uint16_t)st;
            synerr(S);
        }

        st = e >> 8;

        if (code == PS_TOK)
            S->comp.p++;
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

void comp_expected(Session *const S, const uint16_t state)
{
    const char *names[PS_NC * 2];
    const uint8_t n = comp_expnames(state, names);

    for (uint8_t i = 0; i < n; i++) {
        cons_puts(S, names[i]);
        const uint8_t left = n - i - 1;

        if (left)
            cons_puts(S, left == 1 ? " or " : ", ");
    }
}

/* cm_tok: the statement at comp.p. */
static void token(Session *const S)
{
    S->comp.tok = 0; /* an implied LET's: none */
    mark(S);
    dbg(S);

    if (ctrl_stmt(S))
        return;

    const uint8_t c = *S->comp.p;

    if (c == M_VAR) {
        comp_dfa(S, ST_LET);
        return;
    }

    if (c < 0x80)
        synerr(S);

    S->comp.tok = c;
    S->comp.kw = S->comp.p;
    uint16_t st;

    if (c == TK_X) { /* FFh n: its own table, past both */
        st = lexr_ps_entry1[S->comp.p[1]];
        S->comp.p++;
    } else
        st = lexr_ps_entry[c - 0x80];

    if (!st)
        synerr(S);

    comp_curn(&S->comp);
    comp_dfa(S, st);
}

/* cm_nx: the statement compiled, its end next. */
static void ended(Session *const S)
{
    if (S->code.full) /* cm_om: the code is full */
        comp_fail(S, E_OM);

    dbgdrop(S);

    if (!comp_end(S))
        synerr(S);
}

/* The statements after comp.p, to the line's end. */
static void stmts(Session *const S)
{
    for (;;) { /* cm_new: comp.p before a statement */
        S->comp.then = false;

        if (!eosnext(S)) {
            token(S);
            ended(S);
        }

        for (;;) { /* cm_ee: 0 or ' the line ends, ':' another statement, ELSE a one-line IF's */
            uint8_t c = *S->comp.p;

            if (!c || c == TK_REM)
                return;
            if (c == ':')
                break;

            code_op(&S->code, O_NXLN); /* ELSE: the THEN part leaves the line */
            c = comp_curn(&S->comp);

            if (!c)
                synerr(S);
            if (c == M_INT) {
                mark(S);
                comp_dfa(S, ST_GOTO); /* ELSE line */
            } else
                token(S);

            ended(S);
        }
    }
}

void comp_text(Session *const S, const uint8_t *const body)
{
    S->comp.busy = true;
    S->comp.depth = 0;
    S->comp.body = body;
    S->comp.nstmt = 0;
    S->comp.oneif = false;
    S->comp.dg = 0;
    S->comp.dfa = 0xFFFF;
    S->code.iload = S->code.iloadend = S->code.cmpend = nullptr;
    S->code.out = S->code.buf;
    S->code.full = false;
    S->comp.map = S->code.buf + XLIM + COMP_TAIL; /* the map grows down from the top */
    S->code.lim = S->comp.map - COMP_TAIL;
    const uint8_t *colon = comp_label(body); /* a label: on from its ':' */
    S->comp.p = colon ? colon : body - 1;

    if (!setjmp(S->comp.unwind)) {
        stmts(S);
        finish(S);
    }

    S->comp.busy = false;
    S->comp.len = (uint8_t)(S->code.out - S->code.buf);
}

void comp_proc(Session *const S, const uint8_t proc)
{
    S->comp.proc = proc;
    scop_enter(&S->scop, proc ? blok_proc(&S->blok, proc) : nullptr);
}

void comp_line(Session *const S)
{
    S->comp.line = DLINE;
    comp_proc(S, S->comp.dlproc);
    decl_deftype(&S->decl, &S->symb, 0xFFFFFFFF); /* the program's end's DEFtypes */
    S->code.buf = S->comp.xbuf;
    comp_text(S, S->comp.cbuf);
}

/* ---- the actions */

/* The one of four for the value's type. */
static uint8_t by_type(
    Session *const S,
    const uint8_t i,
    const uint8_t f,
    const uint8_t d,
    const uint8_t s
)
{
    return comp_pick(S->code.type, i, f, d, s);
}

/* IF expr THEN|GOTO line or statements: a compare just made becomes its CMPIF, else X.IF. */
static void act_if(Session *const S)
{
    expr_compile(S);
    code_dtest(&S->code);
    uint8_t c = *S->comp.p;

    if (c != TK_THEN && c != TK_GOTO)
        synerr(S);
    if (c == TK_THEN && (!S->comp.p[1] || S->comp.p[1] == TK_REM)) { /* a block IF */
        ctrl_blockif(S);
        return;
    }

    S->comp.oneif = true;
    uint32_t t;

    if (ctrl_else(S, &t))
        ctrl_jf(S, t); /* false: past the ELSE */
    else if (S->code.out == S->code.cmpend)
        *S->code.cmp += O_I_CMPIF - O_I_CMP;
    else /* comp_dtest made a DOUBLE an INT */
        code_op(&S->code, by_type(S, O_I_IF, O_F_IF, O_I_IF, O_S_IF));

    S->comp.then = true;
    const uint8_t kw = *S->comp.p; /* THEN or GOTO */
    c = comp_curn(&S->comp);

    if (kw == TK_GOTO || c == M_INT)
        comp_dfa(S, ST_GOTO);
    else
        token(S);
}

/* FOR's variable: the target of the start's store. */
static void act_fv(Session *const S)
{
    const uint16_t id = comp_name(S);
    const uint8_t info = symb_var(&S->symb, id)->info;

    if (info & VI_ARR)
        synerr(S);

    S->comp.forvar = id;
    comp_target(S, id);

    if (info & SK_DBL) /* a DOUBLE's FOR: not yet */
        mismatch(S);

    const uint8_t kind = info & VK_MASK;

    if (kind == VK_STR)
        mismatch(S);

    S->comp.forkind = kind;
}

/* The value as FOR's variable wants it. */
static void act_fork(Session *const S) { comp_toinfo(S, S->comp.forkind); }

static void act_for(Session *const S, const uint8_t op)
{
    comp_opid(S, S->comp.forkind ? op : op + 1, S->comp.forvar); /* F.FOR, F.FOR1 after the INT's */
    ctrl_forskip(S);
}
static void act_for2(Session *const S) { act_for(S, O_I_FOR); }
static void act_for1(Session *const S) { act_for(S, O_I_FOR1); }

/* NEXT.V id, or NEXT. */
static void act_nv(Session *const S)
{
    if (comp_cur(&S->comp) == M_VAR)
        comp_opid(S, O_NEXT_V, comp_var(S, 0, 0));
    else
        code_op(&S->code, O_NEXT);
}

/* DATA: no code, READ finds the text in the body. */
static void act_data(Session *const S) { S->comp.p = comp_skipdata(S->comp.kw + 1); }

/* REM: the line's end. */
static void act_rem(Session *const S) { done(S); }

/* RUN, RESTORE [line]: CALL, or the line and CALL .L. */
static void runres(Session *const S, const uint8_t k)
{
    if (comp_end(S)) {
        code_call(&S->code, k);
        return;
    }

    S->comp.modlabel = true; /* the module's label */
    comp_lnum(S);
    lnumload(S);
    code_call(&S->code, k + 1);
}
static void act_run(Session *const S) { runres(S, K_RUN); }
static void act_res(Session *const S) { runres(S, K_RES); }

static void act_n0(Session *const S) { S->comp.nargs = 0; }
static void act_n1(Session *const S) { S->comp.nargs++; }
static void act_narg(Session *const S) { code_int(&S->code, S->comp.nargs); }

/* DIM's ( [lo TO] hi {, ..} ): lo hi, or hi and OPTION BASE's, each an INT; how many. */
static uint32_t dimsubs(Session *const S)
{
    comp_open(S);
    uint32_t n = 0;
    S->comp.dmask = 0;

    do {
        expr_compile(S);
        comp_toint(S);

        if (comp_cur(&S->comp) == TK_TO) {
            S->comp.p++;
            expr_compile(S);
            comp_toint(S);
        } else {
            code_int(&S->code, S->decl.base);
            S->comp.dmask |= (uint64_t)1 << (n & 63);
        }

        n++;
    } while (comp_optcomma(S));

    comp_close(S);
    return n;
}

/* DIM's array id: ( subscripts ), the mask as 2 INTs, its id, the count, CALL DIM. A STATIC's
   array is dimensioned once. */
static void dim1(Session *const S, const uint16_t id)
{
    const uint32_t flags = (S->comp.isstatic ? DIM_STATIC : 0) | S->comp.redim;
    const uint32_t n = dimsubs(S);
    code_int(&S->code, (uint32_t)S->comp.dmask);
    code_int(&S->code, (uint32_t)(S->comp.dmask >> 32));
    code_int(&S->code, id);
    code_int(&S->code, n | flags);
    code_call(&S->code, K_DIM);
}

/* Past DIM's AS type [* n]: decl_line's. */
static void dimas(Session *const S)
{
    const uint8_t *p = S->comp.p;

    if (*p != TK_AS)
        return;
    if (p[1] == M_VAR) { /* AS a TYPE */
        S->comp.p = decl_item(p + 1);
        return;
    }

    if (p[1] == TK_X) { /* AS SHORT: AS, FFh n */
        S->comp.p = p + 3;
        return;
    }

    S->comp.p = p + 2;

    if (p[1] == TK_STRING && p[2] == TK_MUL) /* STRING * n */
        S->comp.p += 1 + INT_TOKEN_SIZE;
}

/* A DIM item: the subscripts, its id, n, CALL DIM; a scalar STRING * n: n spaces. */
static void act_dv(Session *const S)
{
    uint16_t id = comp_name(S);

    if (*S->comp.p == '(') {
        const uint16_t name = symb_var(&S->symb, id)->name;
        const uint8_t t = scop_rec(&S->scop, name, true); /* a record array: its own variable */
        if (t)
            id = ref_recarray(S, name, t);

        dim1(S, id);
        dimas(S);
        return;
    }

    dimas(S);
    const uint16_t fix = scop_fix(&S->scop, symb_var(&S->symb, id)->name, false);

    if (!fix || symb_var(&S->symb, id)->info & VI_ARR)
        return;

    code_int(&S->code, fix);
    code_call(&S->code, K_SPACE);
    comp_opid(S, O_S_ST_V, id);
}

/* INPUT's, READ's, GET's value for the target, stored. */
static void readto(Session *const S, uint8_t k)
{
    S->code.type = T_FLT;
    const uint8_t info = S->comp.tgtinfo;

    if (info & SK_DBL && k != K_FGET) {
        k = k == K_FIN ? K_DIN : K_DREAD;
        S->code.type = T_DBL;
    } else if ((info & VK_MASK) == VK_STR) {
        k++;
        S->code.type = T_STR;
    }

    code_call(&S->code, k);
    comp_store(S);
}
static void act_in(Session *const S) { readto(S, K_FIN); }
static void act_rd(Session *const S) { readto(S, K_FREAD); }
static void act_get(Session *const S) { readto(S, K_FGET); }

/* PRINT USING's items: each by its type; US.END, or US.ENDN when ; or , ends the list. */
static void using(Session *const S)
{
    for (;;) {
        if (comp_end(S))
            break;

        expr_compile(S);
        code_call(&S->code, by_type(S, K_USI, K_USF, K_USD, K_USS));

        if (comp_end(S))
            break;

        const uint8_t c = *S->comp.p;

        if (c != ',' && c != ';')
            synerr(S);
        if (eosnext(S)) {
            code_call(&S->code, K_USEN);
            return;
        }
    }

    code_call(&S->code, K_USE);
}

/* PRINT items; PR.NL unless the list ends with ; , TAB( or SPC(. USING: using's items. */
static void act_print(Session *const S)
{
    if (comp_cur(&S->comp) == TK_X && S->comp.p[1] == TK1_USING) {
        S->comp.p += 2; /* USING format; items */
        expr_compile(S);
        comp_tostring(S);
        code_call(&S->code, K_USB);

        if (comp_cur(&S->comp) != ';')
            synerr(S);

        S->comp.p++;
        using(S);
        return;
    }

    if (comp_end(S)) {
        code_call(&S->code, K_PNL);
        return;
    }

    for (;;) {
        const uint8_t c = *S->comp.p;

        if (c == ',')
            code_call(&S->code, K_PZONE);
        else if (c == TK_TAB || c == TK_SPC) {
            S->comp.p++;
            expr_paren(S);
            comp_tobyte(S);
            S->comp.p--;
            code_call(&S->code, c == TK_TAB ? K_PTAB : K_PSPC);
        } else if (c != ';') {
            expr_compile(S);
            code_call(&S->code, by_type(S, K_IPR, K_FPR, K_DPR, K_SPR));

            if (comp_end(S)) {
                code_call(&S->code, K_PNL);
                return;
            }

            continue;
        }

        if (eosnext(S))
            return;
    }
}

/* WRITE items: each by its type, a comma between, PR.NL after. */
static void act_write(Session *const S)
{
    for (;;) {
        if (comp_end(S))
            break;

        expr_compile(S);
        code_call(&S->code, by_type(S, K_WI, K_WF, K_WD, K_WS));

        if (comp_end(S))
            break;

        const uint8_t c = *S->comp.p;

        if (c != ',' && c != ';')
            synerr(S);

        S->comp.p++;
        code_call(&S->code, K_WCOM);
    }

    code_call(&S->code, K_PNL);
}

/* LINE INPUT's target: the line, stored. */
static void act_lin(Session *const S)
{
    code_call(&S->code, K_SLINE);
    S->code.type = T_STR;
    comp_store(S);
}

/* LIST [from][-[to]]: the two numbers, CALL LIST. */
static void act_list(Session *const S)
{
    const uint8_t *const start = S->comp.p;
    const uint8_t c = comp_lnum(S);
    lnumload(S);

    if (c == TK_MINUS) {
        if (eosnext(S))
            S->comp.lnum = 0xFFFF;
        else
            comp_lnum(S);
    } else if (S->comp.p == start)
        S->comp.lnum = 0xFFFF;

    lnumload(S);
    code_call(&S->code, K_LIST);
}

/* GOTO|GOSUB after ON's expression: ON kn, n 0 yet. */
static void act_on(Session *const S)
{
    comp_tobyte(S);
    const uint8_t c = *S->comp.p;

    if (c != TK_GOTO && c != TK_GOSUB) {
        S->comp.dg = DG_GOTO_MISSING;
        synerr(S);
    }

    code_opb(&S->code, O_ON, c == TK_GOSUB ? ON_GOSUB : 0);
    S->comp.onat = S->code.out - 1;
    S->comp.p++;
}

/* One of ON's lines: num addr; n + 1. */
static void act_onl(Session *const S)
{
    comp_lnum(S);
    comp_lref(S);
    (*S->comp.onat)++;
}

static void act_goto(Session *const S)
{
    code_op(&S->code, O_GOTO);
    comp_lref(S);
}

static void act_gosub(Session *const S)
{
    code_op(&S->code, O_GOSUB);
    comp_lref(S);
}

static void act_ret(Session *const S) { code_op(&S->code, O_RETURN); }

/* CALL LOAD, SAVE or VERIFY by the token. */
static void act_fop(Session *const S) { code_call(&S->code, S->comp.tok + K_LOAD - TK_LOAD); }

static void act_lnum(Session *const S) { comp_lnum(S); }

static void (*const actions[A_END - A_BASE])(Session *S) = {
    [A_E - A_BASE] = expr_compile,    [A_LV - A_BASE] = expr_target,
    [A_L - A_BASE] = act_lnum,        [A_TOB - A_BASE] = comp_tobyte,
    [A_TOA - A_BASE] = comp_toaddr,   [A_TOS - A_BASE] = comp_tostring,
    [A_STR - A_BASE] = expr_string,   [A_FV - A_BASE] = act_fv,
    [A_FST - A_BASE] = comp_store,    [A_FK - A_BASE] = act_fork,
    [A_FOR - A_BASE] = act_for2,      [A_FOR1 - A_BASE] = act_for1,
    [A_NV - A_BASE] = act_nv,         [A_IF - A_BASE] = act_if,
    [A_ON - A_BASE] = act_on,         [A_ONL - A_BASE] = act_onl,
    [A_PR - A_BASE] = act_print,      [A_LST - A_BASE] = act_list,
    [A_DV - A_BASE] = act_dv,         [A_DATA - A_BASE] = act_data,
    [A_REM - A_BASE] = act_rem,       [A_LET - A_BASE] = comp_store,
    [A_IN - A_BASE] = act_in,         [A_RD - A_BASE] = act_rd,
    [A_GET - A_BASE] = act_get,       [A_GOTO - A_BASE] = act_goto,
    [A_GOSUB - A_BASE] = act_gosub,   [A_RUN - A_BASE] = act_run,
    [A_RES - A_BASE] = act_res,       [A_RET - A_BASE] = act_ret,
    [A_N0 - A_BASE] = act_n0,         [A_N1 - A_BASE] = act_n1,
    [A_NARG - A_BASE] = act_narg,     [A_FOP - A_BASE] = act_fop,
    [A_SWAP - A_BASE] = ctrl_swap,    [A_TOI - A_BASE] = comp_toint,
    [A_TOD - A_BASE] = comp_todouble, [A_VA - A_BASE] = expr_varaddr,
    [A_WR - A_BASE] = act_write,      [A_LIN - A_BASE] = act_lin,
    [A_LE - A_BASE] = expr_assign,
};
