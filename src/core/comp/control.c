/* Compiles block statements, procedure calls, records' copies, and event and other control statements.
 *
 * # How it works
 * ctrl_stmt takes the statement at comp.p if it is one of these, else returns false. A block
 * statement finds its entry (blok_find) and jumps to the targets blok_pair gave it:
 *   IF c THEN         c, JF NXT
 *   ELSEIF c THEN     JMP END, c, JF NXT
 *   ELSE              JMP END
 *   SELECT CASE e     e to its value's variable #n (n its depth), JMP NXT
 *   CASE tests        JMP END, the tests or'd, JF NXT
 *   DO [WHILE|UNTIL c]    c, JF|JT END
 *   LOOP [WHILE|UNTIL c]  JMP NXT, or c, JT|JF NXT
 *   WHILE c, WEND     c, JF END; JMP NXT
 *   FOR v = a TO b    a to v, b to lim, step (or 1) to step, past, JT END
 *   NEXT [v]          v + step to v, past, JF BODY
 *   EXIT FOR, EXIT DO JMP END
 *
 * A FOR loop's limit and step are hidden variables of its procedure (loopcell), named by the FOR's
 * position; a NEXT's entry names its FOR. past is v > lim, v < lim for a constant step below 0, or
 * PAST, by the step's sign, for one known only when run. An INT's NEXT stepped past its range is
 * Illegal function call (I.STEP.V). The direct line pairs its own FORs and NEXTs.
 *
 * A direct line has no entries: its targets are none (NOTARGET, FFFFFFFFh), an error when run. A
 * call's arguments and a record's references are parsed by refs.c; this file emits their code.
 * Also here: ON ERROR, events, RESUME, COMMON, SWAP, REDIM, ERASE, DEF FN.
 *
 * # asm structure (retired/asm32/comp/ctl.asm)
 * | asm | here | |
 * |---|---|---|
 * | ct_stmt | ctrl_stmt | MIRROR |
 * | ct_bif, ct_else, ct_jf, ct_fskip | ctrl_blockif, ctrl_else, ctrl_jf, ctrl_for | MIRROR; D3 |
 * | ct_fcall, ct_uarg, ct_proc | ref_call, ref_dropargs, ref_proc (refs.c) | MOVED; D1 |
 * | ct_pcall | pcall | MIRROR |
 * | cm_swap | ctrl_swap | MIRROR |
 * | rc_ref, rc_load, rc_avar | ref_record, ref_field, ref_recarray (refs.c) | MOVED |
 * | rc_store | ctrl_recstore | MIRROR |
 * | g_rfet, g_rfoff, g_rffix | an N_REF's rec.et, rec.off, rec.fix | D2 |
 * | g_ctfet, g_ctffix | ctrl.fieldet, ctrl.fieldfix | MIRROR |
 * | ct_none, ct_dseg, ct_def, ct_skip0, ct_redim, ct_erase, ct_onerr, ct_evk, ct_evn, ct_evs, ct_xst, ct_common, cm_skvar, ct_skip, ct_sub, ct_pret | statics none, defseg, deffn, skipparen, redim, erase, onerror, evkind, evnum, evstate, xstmt, common, skipvar, skip, sub, pret | MIRROR |
 * | cm_skas, ct_mids | the asm's skips; mids | MIRROR |
 * | ct_call, ct_name, ct_let, ct_rcopy, ct_scall, ct_pdecl, ct_pchk | call, namestmt, let, reccopy, subcall, declname, checkonly | MIRROR |
 * | ct_args, ct_arg, ct_aend, ct_ptm, rc_nrec, rc_nreca, rc_saddr, rc_whole, rc_path, rc_svar | args, arg, ptm, nrec, place, whole, path, recvar (refs.c) | MOVED |
 * | ct_target, ct_emitjump, ct_ent, ct_tgt, ct_jt, ct_jmp, ct_cond | target, emitjump (ctrl_jump), entry, target, jt, jump, cond | MIRROR |
 * | ct_elseif, ct_belse, ct_do, ct_loop, ct_while, ct_wend, ct_exit, ct_select, ct_var, ct_case, ct_item, ct_cmp, ct_stv, ct_ldop, ct_vop | elseif, blockelse, doloop, loop, whileloop, wend, exitblock, select, selvar, casestmt, caseitem, casecmp, stv, ldop, code_opid | MIRROR |
 * | g_ctnm, g_ctvr, g_cttp, g_cbyv, g_cbyd, g_cpar, g_rfnt | statics; ctrl.selid, ctrl.seltype | MIRROR |
 *
 * - D1. The call is a tree (N_CALL, built by ref_call); comp_step makes it a step.
 * - D2. A record's place is a RecRef on the node, not globals.
 * - D3. The asm's FOR pushes a frame NEXT finds at run time by its variable; here NEXT knows its
 *   FOR when compiled: both are loads, stores, a compare and a jump.
 */
#include "comp/control.h"
#include "comp/blocks.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "comp/expr.h"
#include "comp/fold.h"
#include "comp/gfxstmt.h"
#include "comp/link.h"
#include "comp/sndstmt.h"
#include "comp/import.h"
#include "comp/refs.h"
#include "io/msg.h"
#include "io/repl.h"
#include "mem/scope.h"
#include "mem/symbols.h"
#include "num/double.h"

/* A SELECT's entry's aux, high byte: its value's T_* | SEL_COMPILED (T_INT is 0). */
enum : uint8_t { SEL_COMPILED = 1 };

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

static void skipvar(Comp *const C) { C->p = comp_skipname(C->p); }

/* cm_name with g_vfs: the M_VAR at comp.p as comp_var(shape, 0). */
static uint32_t shaped(Comp *const C, const uint8_t shape)
{
    if (comp_cur(C) != M_VAR)
        synerr(C);

    return comp_var(C, shape, 0);
}

/* ct_skip: to the statement's end: 0, ':' or '. */
static void skip(Comp *const C)
{
    while (*C->p && *C->p != ':' && *C->p != TK_REM)
        C->p = decl_item(C->p);
}

/* SHARED, STATIC n[()] [AS t] {, ..}: entered with the line (decl_line), no code; a CONST's name
   E_RD. */
static void sharedlist(Comp *const C)
{
    C->p++;

    for (;;) {
        comp_notconst(C);

        while (*C->p && *C->p != ':' && *C->p != TK_REM && *C->p != ',')
            C->p = decl_item(C->p);
        if (*C->p != ',')
            return;

        C->p++;
    }
}

/* ct_skip0: past a ( .. ), if there. */
static void skipparen(Comp *const C) { C->p = decl_paren(C->p); }

/* ---- targets and jumps */

/* ct_target: the target in this line at body offset off, mode. */
static uint32_t target(Comp *const C, const uint32_t off, const JumpMode mode)
{
    if (C->line == DLINE)
        return (off * 4 + mode) | PDIR;

    return (off + C->line + HDR) * 4 + mode;
}

Jump ctrl_jump(Comp *const C, const uint32_t t)
{
    Jump j = {.line = NOTARGET};

    if (t != NOTARGET) {
        j.dest = t & 3; /* the mode, till found */
        if (t & PDIR) {
            j.line = DLINE;
            j.body = (t & ~PDIR) >> 2;
        } else {
            /* from a procedure's first line: from the program's, a jump read every line before its
               target's, 30% of a 2600-line program's compile */
            const uint32_t pos = t >> 2;
            const uint32_t near = blok_line_before(C->blok, pos);
            const uint32_t from =
                C->line != DLINE && pos >= C->line && C->line > near ? C->line : near;
            uint32_t off;
            j.line = prog_lineof_from(C->prog, from, pos, &off);
            j.body = off;
        }
    }

    return j;
}

/* ct_ent: the entry of the statement at at; null in the direct line. */
static BlockEnt *entry(Comp *const C, const uint8_t *const at)
{
    if (C->line == DLINE)
        return nullptr;

    BlockEnt *const e = blok_find(C->blok, C->line, (uint32_t)(at - C->body));

    if (!e)
        synerr(C);

    return e;
}

static uint32_t nextof(const BlockEnt *const e) { return e ? e->next : NOTARGET; }
static uint32_t doneof(const BlockEnt *const e) { return e ? e->done : NOTARGET; }

static void jump(Comp *const C, const uint8_t op, const uint32_t t)
{
    comp_go(C, op, ctrl_jump(C, t));
}

static void jmp(Comp *const C, const uint32_t t) { jump(C, O_JMP, t); }

void ctrl_jf(Comp *const C, const uint32_t t) { comp_cond(C, O_I_JF, ctrl_jump(C, t)); }
static void jt(Comp *const C, const uint32_t t) { comp_cond(C, O_I_JT, ctrl_jump(C, t)); }

void ctrl_blockif(Comp *const C)
{
    const BlockEnt *const e = entry(C, C->kw);
    C->p++;
    ctrl_jf(C, nextof(e));
}

bool ctrl_else(Comp *const C, uint32_t *const t)
{
    const uint8_t *p = C->p;
    uint32_t n = 0; /* inner IFs */
    for (;;) {
        const uint8_t c = *p;

        if (!c || c == TK_REM)
            return false;
        if (c == TK_IF)
            n++;
        if (c == TK_ELSE) {
            if (!n)
                break;

            n--;
        }

        p = decl_item(p);
    }

    *t = target(C, (uint32_t)(p + 1 - C->body), TO_STMT);
    return true;
}

/* FOR loop key's hidden cell, which 'L' its limit, 'S' its step: a variable of counter's type, local
   to the procedure. key: the FOR's position; in the direct line, DFOR_KEY and its depth. */
static uint32_t loopcell(
    Comp *const C,
    const uint32_t key,
    const uint8_t which,
    const uint32_t counter
)
{
    const Var v = *symb_var(C->symb, counter); /* the tables move as names are made */
    const uint32_t name = comp_hidden(C, LOOP_NAME, which, key);
    const bool stat = C->proc && blok_proc(C->blok, C->proc)->flags & PF_STAT;
    return symb_find_var(C->symb, name, v.type, v.info & (SK_DBL | VK_MASK), C->proc, stat);
}

enum : uint32_t { DFOR_KEY = 0xFFFF0000 };

/* A loop's step's sign: known when compiled, or only when run. */
typedef enum { STEP_UP, STEP_DOWN, STEP_RUN } StepSign;

/* The sign of the STEP of the FOR at at (its token), as a value of kind; none's is up. */
static StepSign stepsign(Comp *const C, const uint8_t *const at, const uint8_t kind)
{
    const uint8_t *p = at + 1;

    while (*p && *p != ':' && *p != TK_REM && *p != TK_ELSE && *p != TK_STEP)
        p = decl_item(p);

    if (*p != TK_STEP)
        return STEP_UP;

    const uint8_t *const keep = C->p;
    C->p = p + 1;
    Node *const step = ast_new(C->ast, N_CONV);
    const bool ints = kind == VK_INT;
    *step = (Node){
        .kind = N_CONV,
        .op = ints ? CV_INT : CV_FLT,
        .type = ints ? T_INT : T_FLT,
        .a = expr_tree(C, false),
        .end = C->p,
    };

    comp_check(C, step);
    C->p = keep;
    Cell v;

    if (!fold_number(C, step, &v))
        return STEP_RUN;

    return v.u >> 31 ? STEP_DOWN : STEP_UP;
}

/* A FOR loop: its counter, its hidden limit and step, the counter's VK_* (SK_DBL a DOUBLE's) and
   the step's sign. */
typedef struct {
    uint32_t counter, limit, step;
    uint8_t kind;
    StepSign sign;
} Loop;

/* The loop of the FOR at at, key its hidden cells', counter its variable. */
static Loop loopof(
    Comp *const C,
    const uint8_t *const at,
    const uint32_t key,
    const uint32_t counter
)
{
    const uint8_t kind = symb_var(C->symb, counter)->info & (SK_DBL | VK_MASK);

    return (Loop){
        .counter = counter,
        .limit = loopcell(C, key, 'L', counter),
        .step = loopcell(C, key, 'S', counter),
        .kind = kind,
        .sign = stepsign(C, at, kind),
    };
}

static uint8_t loadop(const Loop *const l)
{
    return l->kind == VK_INT ? O_I_LD_V : l->kind & SK_DBL ? O_D_LD_V : O_F_LD_V;
}

static Node *varleaf(Comp *const C, const uint32_t id)
{
    Node *const n = ast_new(C->ast, N_VAR);
    n->id = id;
    n->type = expr_ktype[symb_var(C->symb, id)->info & (SK_DBL | VK_MASK)];
    return n;
}

/* The flag of the counter past the limit: compared one way, or by PAST on the step's sign. */
static void past(Comp *const C, const Loop *const l)
{
    if (l->sign != STEP_RUN) {
        Node *const n = ast_new(C->ast, N_BIN);
        *n = (Node){
            .kind = N_BIN,
            .op = REL | (l->sign == STEP_DOWN ? DC_LT : DC_GT),
            .a = varleaf(C, l->counter),
            .b = varleaf(C, l->limit),
            .end = C->p,
        };

        comp_step(C, n);
        return;
    }

    comp_opid(C, loadop(l), l->counter);
    comp_opid(C, loadop(l), l->limit);
    comp_opid(C, loadop(l), l->step);

    if (l->kind == VK_INT)
        comp_op(C, O_I_PAST);
    else if (l->kind & SK_DBL)
        comp_xop(C, X_D_PAST);
    else
        comp_op(C, O_F_PAST);

    comp_value(C, T_INT);
}

/* Step n the value stored to variable id. */
static void storeat(Comp *const C, Node *const n, const uint32_t id)
{
    ast_wrapat(C->ast, n, N_STORE)->id = id;
}

/* The step before n. */
static Node *before(const Ast *const t, const Node *const n)
{
    Node *p = t->steps;

    while (p->next != n)
        p = p->next;

    return p;
}

/* The steps end with the limit and, if stepped, the step: each to its cell, the step 1 if not; past
   the NEXT (done) when not entered. */
static void forloop(Comp *const C, const uint32_t key, const uint32_t done, const bool stepped)
{
    const Loop l = loopof(C, C->kw, key, C->forvar);
    Node *const last = C->ast->last;

    if (stepped) {
        storeat(C, before(C->ast, last), l.limit);
        storeat(C, last, l.step);
    } else {
        storeat(C, last, l.limit);
        Node *const one = ast_new(C->ast, N_LIT);
        *one = (Node){
            .kind = N_LIT,
            .op = l.kind == VK_INT ? O_I_LD_U8 : O_F_LD_U8,
            .type = l.kind == VK_INT ? T_INT : T_FLT,
            .lit = {1},
        };

        comp_step(C, one);
        storeat(C, one, l.step);
    }

    past(C, &l);
    jt(C, done);
}

void ctrl_for(Comp *const C, const bool stepped)
{
    if (C->line == DLINE) { /* no entries: past NEXT none, an error when not entered */
        if (C->ndfor == DFOR_MAX)
            fault_error(C->fault, E_OM);

        C->dfor[C->ndfor++] = (uint32_t)(C->kw - C->body);
        forloop(C, DFOR_KEY | (C->ndfor - 1), NOTARGET, stepped);
        return;
    }

    const BlockEnt *const e = entry(C, C->kw);
    forloop(C, e->pos, doneof(e), stepped);
}

/* The counter of the FOR at for (its token): its variable, comp.p kept. */
static uint32_t counterof(Comp *const C, const uint8_t *const at)
{
    const uint8_t *const p = C->p;
    C->p = at + 1;
    const uint32_t id = comp_var(C, 0, 0);
    C->p = p;
    return id;
}

void ctrl_next(Comp *const C)
{
    uint32_t key, back;
    const uint8_t *forat;

    if (C->line == DLINE) {
        if (!C->ndfor && C->check) { /* its FOR on another line: its variable read */
            if (comp_cur(C) == M_VAR)
                (void)comp_var(C, 0, 0);

            return;
        }

        if (!C->ndfor)
            fault_error(C->fault, E_NF);

        const uint32_t off = C->dfor[--C->ndfor];
        key = DFOR_KEY | C->ndfor;
        back = target(C, off, TO_NEXT_STMT);
        forat = C->body + off;
    } else {
        const bool more = C->nextkw == C->kw; /* NEXT a, b's b */
        C->nextent = more ? C->nextent->link : entry(C, C->kw);
        C->nextkw = C->kw;

        if (!C->nextent || C->nextent->kind != BK_NEXT)
            synerr(C);
        if (more)
            comp_part(C); /* a's loop leaves to here, as the block pass sent it */

        key = C->nextent->sel->pos;
        back = C->nextent->next;
        forat = at(C->prog->base + key);
    }

    const uint32_t counter = comp_cur(C) == M_VAR ? comp_var(C, 0, 0) : counterof(C, forat);
    const Loop l = loopof(C, forat, key, counter);
    comp_opid(C, loadop(&l), counter);

    if (l.kind & SK_DBL) {
        comp_opid(C, O_D_LD_V, l.step);
        comp_op(C, O_D_ADD);
    } else
        comp_opid(C, l.kind == VK_INT ? O_I_STEP_V : O_F_ADD_V, l.step);

    comp_value(C, l.kind == VK_INT ? T_INT : l.kind & SK_DBL ? T_DBL : T_FLT);
    comp_target(C, counter);
    comp_store(C);
    past(C, &l);
    ctrl_jf(C, back);
}

/* ct_cond: the test after the keyword. */
static void cond(Comp *const C)
{
    C->p++;
    expr_compile(C);
}

static void elseif(Comp *const C)
{
    const BlockEnt *e = entry(C, C->p);
    jmp(C, doneof(e));
    cond(C);

    if (*C->p != TK_THEN)
        synerr(C);

    C->p++;
    ctrl_jf(C, nextof(e));
}

static void blockelse(Comp *const C)
{
    const BlockEnt *e = entry(C, C->p);
    C->p++;
    jmp(C, doneof(e));
}

static void doloop(Comp *const C)
{
    const BlockEnt *e = entry(C, C->p);
    C->p++;

    if (*C->p == TK_WHILE) {
        cond(C);
        ctrl_jf(C, doneof(e));
    } else if (*C->p == TK_UNTIL) {
        cond(C);
        jt(C, doneof(e));
    }
}

static void loop(Comp *const C)
{
    const BlockEnt *e = entry(C, C->p);
    C->p++;

    if (*C->p == TK_WHILE) { /* back when true */
        cond(C);
        jt(C, nextof(e));
    } else if (*C->p == TK_UNTIL) { /* back when false */
        cond(C);
        ctrl_jf(C, nextof(e));
    } else
        jmp(C, nextof(e));
}

static void whileloop(Comp *const C)
{
    const BlockEnt *e = entry(C, C->p);
    cond(C);
    ctrl_jf(C, doneof(e));
}

static void wend(Comp *const C)
{
    const BlockEnt *e = entry(C, C->p);
    C->p++;
    jmp(C, nextof(e));
}

/* EXIT or CONTINUE, its statement at stmt and comp.p past the word: FOR [counter], DO, WHILE.
   The jump is where the block pass sent it: past the loop, or to its closing statement. */
static void leave(Comp *const C, const uint8_t *const stmt)
{
    const BlockEnt *const e = entry(C, stmt);
    const uint8_t c = comp_cur(C);

    if (c != TK_FOR && c != TK_DO && c != TK_WHILE)
        synerr(C); /* at the word after */

    C->p++;

    if (c == TK_FOR && comp_cur(C) == M_VAR)
        C->p = comp_skipname(C->p);

    jmp(C, doneof(e));
}

/* ---- SELECT CASE, SWAP */

/* ct_var: the variable #n of a SELECT at depth n ('S' SWAP's), of type T_*. */
static uint32_t selvar(Comp *const C, const uint8_t depth, const uint8_t type)
{
    const uint8_t nm[2] = {'#', (uint8_t)('0' + depth)};

    const uint32_t name = symb_find_name(C->symb, 0, nm, 2);
    C->ctrl->seltype = type;
    C->scope = 0;
    C->isstatic = false;
    C->ctrl->selid = symb_find_var(
        C->symb,
        name,
        code_pick(type, ET_I4, ET_R4, ET_R8, ET_SD),
        code_pick(type, VK_INT, VK_FLT, SK_DBL, VK_STR),
        0,
        false
    );
    return C->ctrl->selid;
}

/* ct_ldop: selid's load. */
static uint8_t ldop(Comp *const C)
{
    return code_pick(C->ctrl->seltype, O_I_LD_V, O_F_LD_V, O_D_LD_V, O_S_LD_V);
}

/* ct_stv: store to selid. */
static void stv(Comp *const C)
{
    comp_opid(
        C,
        code_pick(C->ctrl->seltype, O_I_ST_V, O_F_ST_V, O_D_ST_V, O_S_ST_V),
        C->ctrl->selid
    );
}

static void select(Comp *const C)
{
    BlockEnt *const e = entry(C, C->p);
    C->p++;

    if (*C->p != TK_CASE)
        synerr(C);

    C->p++;
    expr_compile(C);

    if (comp_type(C) == T_DBL)
        comp_tofloat(C);

    const uint8_t type = comp_type(C);
    uint8_t depth = 0;

    if (e) {
        depth = (uint8_t)e->aux;
        e->aux = (uint16_t)((e->aux & 0xFF) | (type | SEL_COMPILED) << 8);
    }

    selvar(C, depth, type);
    stv(C);
    jmp(C, nextof(e));
}

/* ct_cmp: the SELECT's value, relation m (DC_*), the expression: an INT. */
static void casecmp(Comp *const C, const uint8_t m)
{
    Node *const n = ast_new(C->ast, N_CASECMP); /* a step first: an error's code has the load */
    n->op = m;
    n->id = C->ctrl->selid;
    n->conv = C->ctrl->seltype;
    ast_step(C->ast, n);
    Node *const e = expr_tree(C, false);
    comp_check(C, e);
    n->a = e; /* checked: a mismatch's code has it */

    if (C->ctrl->selfree) { /* its SELECT unseen: of the first item's kind */
        C->ctrl->selfree = false;

        if (ast_type(e) & T_STR) {
            selvar(C, C->ctrl->seldepth, T_STR);
            n->id = C->ctrl->selid;
            n->conv = T_STR;
        }
    }

    if ((C->ctrl->seltype & T_STR) != (ast_type(e) & T_STR))
        mismatch(C);

    n->mode = 1;
    comp_value(C, T_INT);
}

/* ct_item: IS rel e, e TO e, e. */
static void caseitem(Comp *const C)
{
    if (*C->p == TK_IS) {
        static const uint8_t bit[3] = {DC_GT, DC_EQ, DC_LT};

        C->p++;
        uint8_t m = 0;

        for (uint8_t r; (r = (uint8_t)(*C->p - TK_GT)) < 3; C->p++)
            m |= bit[r];
        if (!m)
            synerr(C);

        casecmp(C, m);
        return;
    }

    for (const uint8_t *p = C->p;; p = decl_item(p)) { /* a TO before ',' or the end */
        const uint8_t c = *p;

        if (!c || c == ':' || c == ',') {
            casecmp(C, DC_EQ);
            return;
        }

        if (c == TK_TO)
            break;
    }

    casecmp(C, DC_GT | DC_EQ);

    if (*C->p != TK_TO)
        synerr(C);

    C->p++;
    casecmp(C, DC_EQ | DC_LT);
    comp_op(C, O_I_AND);
    comp_value(C, T_INT);
}

static void casestmt(Comp *const C)
{
    const BlockEnt *const e = entry(C, C->p);
    jmp(C, doneof(e));
    C->p++;

    if (*C->p == TK_ELSE) {
        C->p++;
        return;
    }

    uint8_t depth = 0, type = T_FLT;

    if (e) { /* its SELECT's depth, type */
        const uint16_t aux = e->sel->aux;
        depth = (uint8_t)aux;
        type = (uint8_t)(aux >> 8);

        if (!type)
            synerr(C); /* its SELECT not compiled */
        type &= (uint8_t)~SEL_COMPILED;
    }

    selvar(C, depth, type);
    C->ctrl->seldepth = depth;
    C->ctrl->selfree = !e && C->check;
    caseitem(C);

    while (*C->p == ',') {
        C->p++;
        caseitem(C);
        comp_op(C, O_I_OR);
        comp_value(C, T_INT);
    }

    ctrl_jf(C, nextof(e));
}

/* ---- records */

void ctrl_recstore(Comp *const C)
{
    const uint32_t et = C->ctrl->fieldet;

    if (et == ET_SD) {
        comp_tostring(C);
        Node *const n = comp_op(C, O_SF_ST);
        comp_word(n, (uint16_t)C->tgt); /* the field's offset */
        comp_word(n, C->ctrl->fieldfix);
        return;
    }

    comp_toinfo(C, comp_etkind[et]);
    Node *const n = comp_op(C, O_FL_ST);
    comp_word(n, (uint16_t)C->tgt);
    comp_byte(n, (uint8_t)et); /* a number's */
}

/* Two whole records of one TYPE at comp.p, sep between them, then the statement's end: their
   addresses, a field's offset added; *rec the first's place. false, comp.p kept: not so. */
static bool recpair(Comp *const C, const uint8_t sep, RecRef *const rec)
{
    const uint8_t *const p = C->p;
    Node *const a = comp_cur(C) == M_VAR ? ref_record(C) : nullptr;

    if (a != nullptr && a->rec.et >= ET_REC && comp_cur(C) == sep) {
        C->p++;
        Node *const b = comp_cur(C) == M_VAR ? ref_record(C) : nullptr;

        if (b != nullptr && b->rec.et == a->rec.et && comp_end(C)) {
            *rec = a->rec;
            comp_step(C, expr_addr(C, a, 0));
            comp_step(C, expr_addr(C, b, 0));
            return true;
        }
    }

    C->p = p;
    return false;
}

/* ct_rcopy: a = b of two records of one TYPE: R.COPY; a TYPE with STRINGs or past R.COPY's 16 bits,
   ours: its index, CALL RCOPY. */
static bool reccopy(Comp *const C)
{
    RecRef rec;

    if (!recpair(C, TK_EQ, &rec))
        return false;

    const uint32_t t = rec.et - ET_REC;

    if (decl_dynamic(decl_type(C->decl, t)) || rec.fix > UINT16_MAX) {
        comp_int(C, t);
        comp_call(C, K_RCOPY);
        return true;
    }

    comp_word(comp_xop(C, X_R_COPY), (uint16_t)rec.fix);
    return true;
}

/* SWAP a, b of two records of one TYPE, ours: its index, CALL RSWAP. */
static bool recswap(Comp *const C)
{
    RecRef rec;

    if (!recpair(C, ',', &rec))
        return false;

    comp_int(C, rec.et - ET_REC);
    comp_call(C, K_RSWAP);
    return true;
}

/* A variable at comp.p, else a Syntax error: the grammar leaves SWAP's to ctrl_swap. */
static void need_var(Comp *const C)
{
    if (comp_cur(C) != M_VAR)
        synerr(C);
}

void ctrl_swap(Comp *const C)
{
    if (recswap(C))
        return;
    const uint8_t *const a = C->p;
    need_var(C);
    expr_load(C);
    selvar(C, 'S' - '0', comp_type(C));
    stv(C);

    if (comp_cur(C) != ',')
        synerr(C);

    const uint8_t *const b = ++C->p;
    need_var(C);
    C->p = a; /* a = b */
    expr_target(C);
    C->p = b;
    const uint32_t tgt = C->tgt;
    expr_load(C);

    if (comp_type(C) != C->ctrl->seltype)
        mismatch(C);

    C->tgt = tgt;
    comp_store(C);
    const uint8_t *const end = C->p;
    C->p = b; /* b = #S */
    expr_target(C);
    comp_opid(C, ldop(C), C->ctrl->selid);
    comp_value(C, C->ctrl->seltype);
    comp_store(C);
    C->p = end;
}

/* A whole array's a() or a(:) at p, then '='. */
static bool whole_eq(const uint8_t *const p)
{
    return (p[0] == '(' && p[1] == ')') || (p[0] == TK_X && p[1] == TK1_WHOLE) ? p[2] == TK_EQ : false;
}

/* Ours, arrays of one type, a record's array fields too: their cells, the source first (a call can
   move a record). a() = b() moves (b left empty), CALL AMOVE, as a() = f(..), an array result, the
   call its cell; a(:) = b(:) copies, CALL ACOPY, as a field alone does. */
static bool arrcopy(Comp *const C)
{
    const uint8_t *const start = C->p;
    uint32_t ta, tb;
    Written wa, wb;
    Node *dst = ref_fieldarray(C, &ta, &wa);
    uint32_t a = 0;

    if (dst == nullptr) {
        if (!whole_eq(comp_skipname(C->p)))
            return false;

        a = ref_wholearray(C, &wa);
        ta = symb_var(C->symb, a)->type;
        dst = ref_cell(C, a);
    } else if (comp_cur(C) != TK_EQ) {
        C->p = start;
        return false;
    }

    C->p++;
    const uint8_t *const at = C->p;
    Node *call = nullptr;
    Node *src = ref_fieldarray(C, &tb, &wb);

    if (src == nullptr) {
        const uint32_t b = ref_arrayvalue(C, &call, &wb);

        if (symb_is_record(C->symb, b)) { /* a record result */
            C->p = at;
            fault_error(C->fault, E_TM);
        }

        tb = symb_var(C->symb, b)->type;
        src = call ? call : ref_cell(C, b);
    }

    if (ta != tb) {
        C->p = at;
        fault_error(C->fault, E_TM);
    }

    const bool copy = wb != WR_ARRAY;

    if (!comp_end(C) || copy != (wa != WR_ARRAY)) { /* a() = b(:), a(:) = b(): which is meant */
        C->p = at;
        synerr(C);
    }

    if (a)
        link_given(C, a);

    comp_step(C, src);
    comp_step(C, dst);
    comp_call(C, copy ? K_ACOPY : K_AMOVE);
    return true;
}

/* ---- procedures */

/* QB's keyword n at comp.p, FFh n: stepped past. */
static bool kw1(Comp *const C, const uint8_t n)
{
    if (C->p[0] != TK_X || C->p[1] != n)
        return false;

    C->p += 2;
    return true;
}

/* A SUB's or FUNCTION's parameters, checked: [([BYVAL] name[()] [AS type], ..)]; declared, a
   DECLARE's: SEG and AS ANY too, its names only placeholders (a CONST's passes, as QB's). */
static void params(Comp *const C, const bool declared)
{
    if (comp_cur(C) == '(') {
        C->p++;

        if (comp_cur(C) != ')') {
            do {
                if (comp_cur(C) == TK_BYVAL)
                    C->p++;
                else if (declared)
                    kw1(C, TK1_SEG);
                if (comp_cur(C) != M_VAR)
                    synerr(C);

                if (!declared)
                    comp_notconst(C);

                skipvar(C);

                if (comp_cur(C) == '(') {
                    C->p++;
                    comp_close(C);
                }

                if (declared && comp_cur(C) == TK_AS && C->p[1] == TK_X && C->p[2] == TK1_ANY)
                    C->p += 3;
                else
                    comp_astype(C);
            } while (comp_optcomma(C));
        }

        comp_close(C);
    }
}

/* A FUNCTION's AS type [()], ours, name its name: a scalar's, a record's, () an array's; none
   with a suffix. */
static void result_as(Comp *const C, const uint8_t *const name)
{
    if (comp_cur(C) != TK_AS)
        return;
    if (comp_suffixof(name) || (C->p[1] == TK_STRING && C->p[2] == TK_MUL))
        synerr(C);

    comp_astype(C);

    if (comp_cur(C) == '(') {
        C->p++;
        comp_close(C);
    }
}

/* A SUB's or FUNCTION's parameters, then a FUNCTION's AS type, name its name; declared: as params. */
static void header(Comp *const C, const bool fn, const uint8_t *const name, const bool declared)
{
    params(C, declared);

    if (fn)
        result_as(C, name);
}

/* DECLARE SUB|FUNCTION name [CDECL] [ALIAS "name"] [(parameters)] [AS type]: entered with the line
   (decl_line), no code; checked as the definition's header. */
static void declare(Comp *const C)
{
    const bool fn = C->p[1] == TK_FUNCTION;

    if ((!fn && C->p[1] != TK_SUB) || C->p[2] != M_VAR)
        synerr(C);

    C->p += 2;
    const uint8_t *const name = C->p;
    skipvar(C);
    kw1(C, TK1_CDECL);

    if (kw1(C, TK1_ALIAS)) {
        if (comp_cur(C) != '"')
            synerr(C);

        C->p = decl_item(C->p);
    }

    header(C, fn, name, true);
}

/* FUNCTION pr's record or array result emptied, ours, as a number's starts at 0: its body's first
   code. */
static void result_empty(Comp *const C, const Proc *const pr)
{
    const uint32_t id = blok_resvar(pr);

    if (!symb_is_record(C->symb, id)) {
        comp_int(C, id);
        comp_call(C, K_ARESET);
        return;
    }

    Node *const r = ref_recplace(C, id);
    comp_step(C, r);
    comp_int(C, (uint32_t)(r->rec.et - ET_REC));
    comp_call(C, K_RCLR);
}

/* ct_pdecl: the name at comp.p a SUB's or FUNCTION's that a DECLARE or a definition named. */
static bool declname(Comp *const C)
{
    return *C->p == M_VAR &&
           symb_name(C->symb, comp_nameof(C->p))->flags & (NM_SUB | NM_FUNC | NM_FN);
}

/* ct_pchk: in the IDE's check of a line alone, a procedure its declaration named passes:
   blok_procs has it only once linked. */
static bool checkonly(Comp *const C) { return C->check && declname(C); }

/* Procedure idx's call, its arguments as form writes them. */
static void pcall(
    Comp *const C,
    const uint8_t *const name,
    const uint32_t idx,
    const Proc *const pr,
    const ArgsForm form
)
{
    comp_step(C, ref_call(C, name, idx, pr, form));
}

/* CALL n [( args )]. */
static void call(Comp *const C)
{
    C->p++;
    const uint8_t *const name = C->p;
    uint32_t idx;
    const Proc *const pr = ref_proc(C, &idx);

    if (!pr) {
        if (!declname(C))
            synerr(C);
        if (C->check) {
            skip(C);
            return;
        }

        fault_error(C->fault, E_SND); /* DECLAREd only */
    }

    pcall(C, name, idx, pr, ARGS_PAREN);
}

/* ct_scall: n args, a SUB's call without CALL; another name is not one. */
static bool subcall(Comp *const C)
{
    const uint8_t *const start = C->p;
    uint32_t idx;
    const Proc *const pr = ref_proc(C, &idx);

    if (pr) {
        if (pr->kind == PK_SUB) {
            ArgsForm form = ARGS_NONE;

            if (!comp_end(C)) {
                if (comp_cur(C) == TK_EQ) /* a SUB's name assigned */
                    synerr(C);

                form = ARGS_BARE;
            }

            pcall(C, start, idx, pr, form);
            return true;
        }

        /* f = e: in FUNCTION f only; a line checked alone is at the module, its place the link's */
        if (idx != C->proc && comp_cur(C) == TK_EQ && !C->check)
            fault_error(C->fault, E_VR);
    }

    C->p = start;

    if (checkonly(C)) {
        if (*comp_skipname(C->p) == TK_EQ) /* f = e: a FUNCTION's result, checked as any */
            return false;
        if (!(symb_name(C->symb, comp_nameof(C->p))->flags & NM_SUB)) {
            skip(C);
            return true;
        }

        skipvar(C); /* a SUB's call: its arguments checked, their count the link's */
        ref_dropargs(C, false);
        return true;
    }

    if (*C->p != M_VAR)
        return false;

    const Name *const n = symb_name(C->symb, comp_nameof(C->p));

    if (!(n->flags & NM_SUB) || n->proc)
        return false;

    skipvar(C); /* a SUB DECLAREd only */
    if (!comp_end(C) && comp_cur(C) == TK_EQ)
        synerr(C);

    C->p = start;
    fault_error(C->fault, E_SND);
}

/* SUB, FUNCTION: run into, passed: JMP past its END. */
static void sub(Comp *const C)
{
    const bool fn = *C->p++ == TK_FUNCTION;
    const uint8_t *const name = C->p;
    uint32_t idx;
    const Proc *const pr = ref_proc(C, &idx);

    if (pr)
        jmp(C, pr->skip);
    else if (!C->check)
        synerr(C);
    else if (comp_cur(C) == M_VAR) /* a new one: the IDE's line check, completion's probe */
        skipvar(C);
    else
        synerr(C);

    header(C, fn, name, false);

    if (comp_cur(C) == TK_STATIC)
        C->p++;
    if (pr && pr->flags & PF_RESARG)
        result_empty(C, pr);
}

/* PRET for procedure proc: its result's cell link's. */
static void ret(Comp *const C, const uint32_t proc)
{
    Node *const n = comp_op(C, O_PRET);
    comp_dword(n, proc);
    comp_dword(n, 0);
}

/* END or EXIT SUB, FUNCTION, DEF: PRET. */
static void pret(Comp *const C)
{
    C->p += 2;
    ret(C, C->proc);
}

/* DEF FNn [(params)]: passed as a FUNCTION; with = e, JMP past it, then the result e, PRET. */
static void deffn(Comp *const C)
{
    const uint8_t *const name = ++C->p;
    uint32_t idx;
    const Proc *const pr = ref_proc(C, &idx);

    if (!pr) {
        if (!checkonly(C))
            synerr(C);

        skip(C);
        return;
    }

    jmp(C, pr->skip);
    skipparen(C);

    if (comp_cur(C) != TK_EQ) {
        skip(C);
        return;
    }

    const uint8_t *const eq = C->p;
    C->p = name;
    comp_target(C, shaped(C, 1)); /* its result */
    C->p = eq + 1;
    expr_compile(C);
    comp_store(C);
    ret(C, idx);
}

/* DEF SEG [= s]: no code, flat; DEF FN. */
static bool defseg(Comp *const C)
{
    if (C->p[1] == TK_X && C->p[2] == TK1_SEG) {
        skip(C);
        return true;
    }

    if (C->p[1] != M_VAR)
        return false;

    deffn(C);
    return true;
}

/* ---- other statements */

/* REDIM [PRESERVE]: DIM's grammar, each CALL DIM marked REDIM's. */
/* DIM PUBLIC, ours: DIM SHARED's, the module's exports (comp/bind.c). */
static void dimpublic(Comp *const C)
{
    if (C->proc) {
        C->dg = DG_ILLEGAL_INSIDE_SUB_OR_FUNCTION;
        synerr(C);
    }

    C->p += 3;
    comp_dfa(C, lexr_ps_entry[TK_DIM - 0x80]);
}

static void redim(Comp *const C)
{
    C->p++;
    C->redim = DIM_REDIM;

    if (C->p[0] == TK_X && C->p[1] == TK1_PRESERVE) {
        C->p += 2;
        C->redim = DIM_REDIM | DIM_PRESERVE;
    }

    comp_dfa(C, lexr_ps_entry[TK_DIM - 0x80]);
    C->redim = 0;
}

/* ERASE a {, a}: each array's cell, CALL ERASE. */
static void erase(Comp *const C)
{
    C->p++;

    do {
        uint32_t type;
        Node *const f = ref_arrayfield(C, &type, WH_FIXED); /* m.a, ours */
        comp_step(C, f ? f : ref_cell(C, ref_arrayof(C, shaped(C, 2))));
        comp_call(C, K_ERASE);
    } while (comp_optcomma(C));
}

/* ct_evk: the TK_X word at comp.p an event's: its EK_*, comp.p past it. */
static bool evkind(Comp *const C, uint8_t *const kind)
{
    switch (C->p[1]) {
    case TK1_TIMER:
        *kind = EK_TIMER;
        break;

    case TK1_KEY:
        *kind = EK_KEY;
        break;

    case TK1_COM:
        *kind = EK_COM;
        break;

    case TK1_PEN:
        *kind = EK_PEN;
        break;

    case TK1_STRIG:
        *kind = EK_STRIG;
        break;

    case TK1_PLAY:
        *kind = EK_PLAY;
        break;

    case TK1_UEVENT:
        *kind = EK_UEVENT;
        break;

    default:
        return false;
    }

    C->p += 2;
    return true;
}

/* ct_evn: the event's (n): KEY's, COM's, STRIG's; TIMER's and PLAY's in their ON; else 0. */
static void evnum(Comp *const C, const uint8_t kind)
{
    if (kind != EK_PEN && kind != EK_UEVENT) {
        if (comp_cur(C) == '(') {
            expr_paren(C);
            comp_toint(C);
            return;
        }

        if (kind != EK_TIMER && kind != EK_PLAY)
            synerr(C);
    }

    comp_int(C, 0);
}

/* ON [LOCAL] ERROR GOTO line: ONERR or ONLERR num addr; ON event(n) GOSUB line: ONEV kind num
   addr; another ON is not one. */
static bool onerror(Comp *const C)
{
    const uint8_t *const start = C->p++;

    if (comp_cur(C) != TK_X) {
        C->p = start;
        return false;
    }

    uint8_t kind;

    if (evkind(C, &kind)) {
        evnum(C, kind);

        if (comp_cur(C) != TK_GOSUB)
            synerr(C);

        C->p++;
        C->modlabel = true; /* an event's handler: the module's */
        comp_lnum(C);
        comp_byte(comp_xjump(C, X_ONEV), kind);
        return true;
    }

    uint8_t x = X_ONERR;

    if (C->p[1] == TK1_LOCAL) { /* PDS's ON LOCAL ERROR */
        C->p += 2;

        if (comp_cur(C) != TK_X)
            synerr(C);

        x = X_ONLERR;
    }

    if (C->p[1] != TK1_ERROR) {
        C->p = start;
        return false;
    }

    C->p += 2;

    if (comp_cur(C) != TK_GOTO) {
        C->dg = DG_GOTO_MISSING;
        synerr(C);
    }

    C->p++;
    C->modlabel = x == X_ONERR; /* ON ERROR's handler: the module's */
    comp_lnum(C);
    comp_xjump(C, x);
    return true;
}

/* ct_evs: event(n) ON | OFF | STOP: CALL EVST ( n kind*4+state -- ); KEY n, s: CALL KEYDEF; KEY
   ON, OFF, LIST: the screen's, nothing. */
static bool evstate(Comp *const C)
{
    uint8_t kind;

    if (!evkind(C, &kind))
        return false;
    if (kind == EK_KEY) {
        const uint8_t c = comp_cur(C);

        if (c == TK_ON || c == TK_LIST) {
            C->p++;
            return true;
        }

        if (c == TK_X && C->p[1] == TK1_OFF) {
            C->p += 2;
            return true;
        }

        if (c != '(') {
            expr_compile(C);
            comp_toint(C);
            comp_comma(C);
            expr_compile(C);
            comp_tostring(C);
            comp_call(C, K_KEYDEF);
            return true;
        }
    }

    if (kind == EK_STRIG && comp_cur(C) != '(') { /* BASICA's STRIG ON, OFF: the joystick read */
        const uint8_t c = comp_cur(C);

        if (c != TK_ON && !(c == TK_X && C->p[1] == TK1_OFF))
            synerr(C);

        C->p += c == TK_ON ? 1 : 2;
        return true;
    }

    evnum(C, kind);
    const uint8_t c = comp_cur(C);
    uint8_t state;

    if (c == TK_X) {
        if (C->p[1] != TK1_OFF)
            synerr(C);

        C->p++;
        state = ES_OFF;
    } else if (c == TK_ON)
        state = ES_ON;
    else if (c == TK_STOP)
        state = ES_STOP;
    else
        synerr(C);

    C->p++;
    comp_int(C, kind * 4u + state);
    comp_call(C, K_EVST);
    return true;
}

/* COMMON [SHARED] [/block/] v[()] [AS type] {, ..}: each variable's id, CALL CMN. */
static void common(Comp *const C)
{
    C->p += 2;

    if (comp_cur(C) == TK_SHARED)
        C->p++;
    if (comp_cur(C) == TK_DIV) {
        C->p++;

        if (comp_cur(C) != M_VAR)
            synerr(C);

        skipvar(C);

        if (comp_cur(C) != TK_DIV)
            synerr(C);

        C->p++;
    }

    do {
        const uint32_t id = comp_name(C);

        if (comp_cur(C) == '(') {
            C->p++;

            if (comp_cur(C) != ')')
                synerr(C);

            C->p++;
            link_given(C, id); /* by the program that CHAINed here */
        }

        comp_astype(C);

        comp_int(C, id);
        comp_call(C, K_CMN);
    } while (comp_optcomma(C));
}

/* RESUME [NEXT | 0 | line]: RESUME mode num addr. */
static void resume(Comp *const C)
{
    C->p += 2;
    C->lnum = 0;
    C->jline = 0;
    ResumeMode mode = RESUME_AGAIN;

    if (comp_cur(C) == TK_NEXT) {
        C->p++;
        mode = RESUME_NEXT;
    } else if (!comp_end(C)) {
        comp_lnum(C);

        if (C->lnum || C->jline) /* not RESUME 0 */
            mode = RESUME_LINE;
    }

    comp_byte(comp_xjump(C, X_RESUME), mode);
}

/* IOCTL [#]n, s$: CALL IOCTL. */
static void ioctl(Comp *const C)
{
    C->p += 2;

    if (comp_cur(C) == '#')
        C->p++;

    expr_compile(C);
    comp_toint(C);
    comp_comma(C);
    expr_compile(C);
    comp_tostring(C);
    comp_call(C, K_IOCTL);
}

/* LPRINT [USING] list: PRINT's, to the printer: CALL LPRT, PRINT's code, CALL FICLR. */
static void lprint(Comp *const C)
{
    C->p += 2;
    comp_call(C, K_LPRT);
    comp_dfa(C, lexr_ps_entry[TK_PRINT - 0x80]);
    comp_call(C, K_FICLR);
}

static bool xstmt(Comp *const C)
{
    switch (C->p[1]) {
    case TK1_LPRINT:
        lprint(C);
        return true;

    case TK1_IOCTL:
        ioctl(C);
        return true;

    case TK1_COMMON:
        common(C);
        return true;

    case TK1_DEFSHORT:
    case TK1_DEFBYTE:
        skip(C);
        return true;

    case TK1_RESUME:
        resume(C);
        return true;

    case TK1_CONTINUE:
        C->p += 2;
        leave(C, C->p - 2);
        return true;

    case TK1_IMPORT:
    case TK1_FROM:
        impt_compile(C);
        return true;
    }

    return evstate(C);
}

/* MID$(v, i [, n]) = s: v = its value with s over it. */
static void mids(Comp *const C)
{
    C->p++;
    comp_open(C);
    const uint8_t *const v = C->p;
    expr_target(C);

    if ((C->tgtinfo & VK_MASK) != VK_STR)
        mismatch(C);

    const uint32_t tgt = C->tgt;
    C->p = v;
    expr_load(C);
    comp_comma(C);
    expr_compile(C);
    comp_toint(C);

    if (comp_optcomma(C)) {
        expr_compile(C);
        comp_toint(C);
    } else
        comp_int(C, SMAX);

    comp_close(C);
    comp_equals(C);
    expr_compile(C);
    comp_tostring(C);
    comp_call(C, K_MIDS);
    comp_value(C, T_STR);
    C->tgt = tgt;
    comp_store(C);
}

/* A statement starting with a name: a TYPE's field (its bounds ours), a record's copy, a SUB's
   call. */
static bool namestmt(Comp *const C)
{
    const uint8_t *q = comp_skipname(C->p);
    FieldDim dims[MAXDIMS];
    uint8_t nd;

    if (*q == '(')
        q = decl_dims(C->scop, C->module, C->proc, q, 0, dims, &nd);
    if (q && *q == TK_AS) {
        skip(C);
        return true;
    }

    return arrcopy(C) || reccopy(C) || subcall(C) || snds_named(C);
}

/* LET: an array's or a record's copy, else not one. */
static bool let(Comp *const C)
{
    C->p++;

    if (*C->p == M_VAR && (arrcopy(C) || reccopy(C)))
        return true;

    C->p--;
    return false;
}

bool ctrl_stmt(Comp *const C)
{
    if (gstm_stmt(C) || snds_stmt(C))
        return true;

    const uint8_t c = C->p[0];
    const uint8_t n = C->p[1];

    switch (c) {
    case TK_DIM:
        if (n != TK_X || C->p[2] != TK1_PUBLIC)
            break;

        dimpublic(C);
        return true;

    case TK_ELSEIF:
        elseif(C);
        return true;

    case TK_ELSE:
        blockelse(C);
        return true;

    case TK_ENDIF:
        C->p++;
        return true;

    case TK_SELECT:
        select(C);
        return true;

    case TK_CASE:
        casestmt(C);
        return true;

    case TK_DO:
        doloop(C);
        return true;

    case TK_LOOP:
        loop(C);
        return true;

    case TK_WHILE:
        whileloop(C);
        return true;

    case TK_WEND:
        wend(C);
        return true;

    case M_VAR:
        return namestmt(C);

    case TK_LET:
        return let(C);

    case TK_CALL:
        call(C);
        return true;

    case TK_SUB:
    case TK_FUNCTION:
        sub(C);
        return true;

    case TK_DEF:
        return defseg(C);

    case TK_MID:
        mids(C);
        return true;

    case TK_ON:
        return onerror(C);

    case TK_REDIM:
        redim(C);
        return true;

    case TK_ERASE:
        erase(C);
        return true;

    case TK_X:
        return xstmt(C);

    case TK_SHARED:
    case TK_STATIC:
        sharedlist(C);
        return true;

    case TK_DECLARE:
        declare(C);
        return true;

    case TK_TYPE:
    case TK_OPTION:
    case TK_DEFINT:
    case TK_DEFLNG:
    case TK_DEFSNG:
    case TK_DEFDBL:
    case TK_DEFSTR:
        skip(C);
        return true; /* entered with the line (decl_line), no code */
    case TK_EXIT:
        if (n == TK_SUB || n == TK_FUNCTION || n == TK_DEF)
            pret(C);
        else {
            C->p++;
            leave(C, C->p - 1);
        }

        return true;

    case TK_END:
        if (C->line != DLINE && prog_line(C->prog, C->line)->flags & LF_TOP) { /* past a module's */
            C->p++;
            comp_xop(C, X_MODEND);
            return true;
        }
        if (n == TK_IF || n == TK_SELECT || n == TK_TYPE) { /* no code */
            C->p += 2;
            return true;
        }

        if (n == TK_SUB || n == TK_FUNCTION || n == TK_DEF) {
            pret(C);
            return true;
        }
    }

    return false;
}
