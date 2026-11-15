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
 * # asm structure (src/comp/ctl.asm)
 * | asm | here | |
 * |---|---|---|
 * | ct_stmt | ctrl_stmt | MIRROR |
 * | ct_bif, ct_else, ct_jf, ct_fskip | ctrl_blockif, ctrl_else, ctrl_jf, ctrl_for | MIRROR; D3 |
 * | ct_fcall, ct_uarg, ct_proc | ref_call, ref_dropargs, ref_proc (refs.c) | MOVED; D1 |
 * | ct_pcall | pcall | MIRROR |
 * | ct_undef, cm_swap | ctrl_undef, ctrl_swap | MIRROR |
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
#include "comp/sndstmt.h"
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
static uint16_t shaped(Comp *const C, const uint8_t shape)
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

/* ct_skip0: past a ( .. ), if there. */
static void skipparen(Comp *const C)
{
    if (*C->p != '(')
        return;

    uint32_t n = 0;

    for (;;) {
        const uint8_t c = *C->p;

        if (!c)
            return;
        if (c == '(')
            n++;

        C->p = decl_item(C->p);

        if (c == ')' && !--n)
            return;
    }
}

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
            uint32_t off;
            j.line = prog_lineof(C->prog, t >> 2, &off);
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
static uint16_t loopcell(
    Comp *const C,
    const uint32_t key,
    const uint8_t which,
    const uint16_t counter
)
{
    const Var v = *symb_var(C->symb, counter); /* the tables move as names are made */
    uint8_t text[2 + 8] = {LOOP_NAME, which};

    for (uint32_t i = 0; i < 8; i++)
        text[2 + i] = "0123456789ABCDEF"[(key >> (28 - 4 * i)) & 15];

    const uint16_t name = symb_find_name(C->symb, text, sizeof text);
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
    uint16_t counter, limit, step;
    uint8_t kind;
    StepSign sign;
} Loop;

/* The loop of the FOR at at, key its hidden cells', counter its variable. */
static Loop loopof(
    Comp *const C,
    const uint8_t *const at,
    const uint32_t key,
    const uint16_t counter
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

static Node *varleaf(Comp *const C, const uint16_t id)
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
    comp_op(C, l->kind == VK_INT ? O_I_PAST : l->kind & SK_DBL ? O_D_PAST : O_F_PAST);
    comp_value(C, T_INT);
}

/* Step n the value stored to variable id. */
static void storeat(Comp *const C, Node *const n, const uint16_t id)
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
static uint16_t counterof(Comp *const C, const uint8_t *const at)
{
    const uint8_t *const p = C->p;
    C->p = at + 1;
    const uint16_t id = comp_var(C, 0, 0);
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
        C->nextent = C->nextkw == C->kw ? C->nextent->link : entry(C, C->kw);
        C->nextkw = C->kw;

        if (!C->nextent || C->nextent->kind != BK_NEXT)
            synerr(C);

        key = C->nextent->sel->pos;
        back = C->nextent->next;
        forat = at(C->prog->base + key);
    }

    const uint16_t counter = comp_cur(C) == M_VAR ? comp_var(C, 0, 0) : counterof(C, forat);
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

/* EXIT FOR, EXIT DO. */
static void exitblock(Comp *const C)
{
    const BlockEnt *const e = entry(C, C->p);
    const uint8_t c = C->p[1];
    C->p++;

    if (c != TK_FOR && c != TK_DO)
        synerr(C); /* at the word after EXIT */

    C->p++;
    jmp(C, doneof(e));
}

/* ---- SELECT CASE, SWAP */

/* ct_var: the variable #n of a SELECT at depth n ('S' SWAP's), of type T_*. */
static uint16_t selvar(Comp *const C, const uint8_t depth, const uint8_t type)
{
    const uint8_t nm[2] = {'#', (uint8_t)('0' + depth)};

    const uint16_t name = symb_find_name(C->symb, nm, 2);
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
    caseitem(C);

    while (*C->p == ',') {
        C->p++;
        caseitem(C);
        comp_op(C, O_I_OR);
        comp_value(C, T_INT);
    }

    ctrl_jf(C, nextof(e));
}

void ctrl_swap(Comp *const C)
{
    const uint8_t *const a = C->p;
    expr_load(C);
    selvar(C, 'S' - '0', comp_type(C));
    stv(C);

    if (comp_cur(C) != ',')
        synerr(C);

    const uint8_t *const b = ++C->p;
    C->p = a; /* a = b */
    expr_target(C);
    C->p = b;
    const uint16_t tgt = C->tgt;
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

/* ---- records */

void ctrl_recstore(Comp *const C)
{
    const uint8_t et = C->ctrl->fieldet;

    if (et == ET_SD) {
        comp_tostring(C);
        Node *const n = comp_op(C, O_SF_ST);
        comp_word(n, C->tgt);
        comp_word(n, C->ctrl->fieldfix);
        return;
    }

    comp_toinfo(C, comp_etkind[et]);
    Node *const n = comp_op(C, O_FL_ST);
    comp_word(n, C->tgt);
    comp_byte(n, et);
}

/* ct_rcopy: a = b of two records of one TYPE: their addresses, R.COPY. */
static bool reccopy(Comp *const C)
{
    const uint8_t *const p = C->p;
    Node *const a = ref_record(C);

    if (a != nullptr && a->rec.et >= ET_REC && comp_cur(C) == TK_EQ) {
        const RecRef rec = a->rec;
        C->p++;
        Node *const b = comp_cur(C) == M_VAR ? ref_record(C) : nullptr;

        if (b != nullptr && b->rec.et == rec.et && comp_end(C)) {
            comp_step(C, a);
            comp_step(C, b);
            comp_word(comp_op(C, O_R_COPY), rec.fix);
            return true;
        }
    }

    C->p = p;
    return false;
}

/* ---- procedures */

/* A SUB's or FUNCTION's parameters, checked: [(name[()] [AS type], ..)] [STATIC]. */
static void params(Comp *const C)
{
    if (comp_cur(C) == '(') {
        C->p++;

        if (comp_cur(C) != ')') {
            do {
                if (comp_cur(C) != M_VAR)
                    synerr(C);

                skipvar(C);

                if (comp_cur(C) == '(') {
                    C->p++;
                    comp_close(C);
                }

                comp_astype(C);
            } while (comp_optcomma(C));
        }

        comp_close(C);
    }

    if (comp_cur(C) == TK_STATIC)
        C->p++;
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

void ctrl_undef(Comp *const C, const uint8_t err)
{
    comp_int(C, err);
    comp_call(C, K_ERROR);
}

/* Procedure idx's call, its arguments as form writes them. */
static void pcall(Comp *const C, const uint8_t idx, const Proc *const pr, const ArgsForm form)
{
    comp_step(C, ref_call(C, idx, pr, form));
}

/* CALL n [( args )]. */
static void call(Comp *const C)
{
    C->p++;
    uint8_t idx;
    const Proc *const pr = ref_proc(C, &idx);

    if (!pr) {
        if (!declname(C))
            synerr(C);
        if (C->check) {
            skip(C);
            return;
        }

        skipvar(C); /* DECLAREd only: its arguments, then Subprogram not defined when run */
        if (*C->p == '(') {
            C->p++;
            ref_dropargs(C, true);
            comp_close(C);
        }

        ctrl_undef(C, QB_NO_SUB);
        return;
    }

    pcall(C, idx, pr, ARGS_PAREN);
}

/* ct_scall: n args, a SUB's call without CALL; another name is not one. */
static bool subcall(Comp *const C)
{
    const uint8_t *const start = C->p;
    uint8_t idx;
    const Proc *const pr = ref_proc(C, &idx);

    if (pr) {
        if (pr->kind == PK_SUB) {
            ArgsForm form = ARGS_NONE;

            if (!comp_end(C)) {
                if (comp_cur(C) == TK_EQ) /* a SUB's name assigned */
                    synerr(C);

                form = ARGS_BARE;
            }

            pcall(C, idx, pr, form);
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
    if (!comp_end(C)) {
        if (comp_cur(C) == TK_EQ)
            synerr(C);

        ref_dropargs(C, false);
    }

    ctrl_undef(C, QB_NO_SUB);
    return true;
}

/* SUB, FUNCTION: run into, passed: JMP past its END. */
static void sub(Comp *const C)
{
    C->p++;
    uint8_t idx;
    const Proc *const pr = ref_proc(C, &idx);

    if (pr)
        jmp(C, pr->skip);
    else if (!C->check)
        synerr(C);
    else if (comp_cur(C) == M_VAR) /* a new one: the IDE's line check, completion's probe */
        skipvar(C);
    else
        synerr(C);

    if (C->check)
        params(C);
    else
        skip(C);
}

/* END or EXIT SUB, FUNCTION, DEF: PRET. */
static void pret(Comp *const C)
{
    C->p += 2;
    comp_op(C, O_PRET);
}

/* DEF FNn [(params)]: passed as a FUNCTION; with = e, JMP past it, then the result e, PRET. */
static void deffn(Comp *const C)
{
    const uint8_t *const name = ++C->p;
    uint8_t idx;
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
    comp_op(C, O_PRET);
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

/* ERASE a {, a}: each array's id, CALL ERASE. */
static void erase(Comp *const C)
{
    C->p++;

    do {
        uint16_t id = shaped(C, 2);
        const uint16_t name = symb_var(C->symb, id)->name;
        const uint8_t t = scop_rec(C->scop, name, true); /* a record array: its own variable */
        if (t)
            id = ref_recarray(C, name, t);

        comp_int(C, id);
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
        comp_byte(comp_jump(C, O_ONEV), kind);
        return true;
    }

    uint8_t op = O_ONERR;

    if (C->p[1] == TK1_LOCAL) { /* PDS's ON LOCAL ERROR */
        C->p += 2;

        if (comp_cur(C) != TK_X)
            synerr(C);

        op = O_ONLERR;
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
    C->modlabel = op == O_ONERR; /* ON ERROR's handler: the module's */
    comp_lnum(C);
    comp_jump(C, op);
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
        const uint16_t id = comp_name(C);

        if (comp_cur(C) == '(') {
            C->p++;

            if (comp_cur(C) != ')')
                synerr(C);

            C->p++;
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

    comp_byte(comp_jump(C, O_RESUME), mode);
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

static bool xstmt(Comp *const C)
{
    switch (C->p[1]) {
    case TK1_IOCTL:
        ioctl(C);
        return true;

    case TK1_COMMON:
        common(C);
        return true;

    case TK1_DEFSHORT:
        skip(C);
        return true;

    case TK1_RESUME:
        resume(C);
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

    const uint16_t tgt = C->tgt;
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

/* A statement starting with a name: a TYPE's field, a record's copy, a SUB's call. */
static bool namestmt(Comp *const C)
{
    if (*comp_skipname(C->p) == TK_AS) {
        skip(C);
        return true;
    }

    return reccopy(C) || subcall(C);
}

/* LET: a record's copy, else not one. */
static bool let(Comp *const C)
{
    C->p++;

    if (*C->p == M_VAR && reccopy(C))
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

    case TK_TYPE:
    case TK_DECLARE:
    case TK_OPTION:
    case TK_SHARED:
    case TK_STATIC:
        skip(C);
        return true; /* entered with the line (decl_line), no code */
    case TK_EXIT:
        if (n == TK_SUB || n == TK_FUNCTION || n == TK_DEF)
            pret(C);
        else
            exitblock(C);

        return true;

    case TK_END:
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
