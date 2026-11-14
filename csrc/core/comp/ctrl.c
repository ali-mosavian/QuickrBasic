/* Block statements, procedures' calls, records' references (comp/ctl.asm). */
#include "comp/ctrl.h"
#include "session.h"
#include "comp/blok.h"
#include "comp/decl.h"
#include "comp/dfa.h"
#include "comp/expr.h"
#include "io/msg.h"
#include "io/repl.h"
#include "mem/scop.h"
#include "mem/symb.h"
#include "num/dubl.h"


/* A SELECT's entry's aux, high byte: its value's T_* | SEL_COMPILED (T_INT is 0). */
enum : uint8_t { SEL_COMPILED = 1 };


[[noreturn]] static void synerr(Session *const S) { repl_error(S, E_SN); }
[[noreturn]] static void mismatch(Session *const S) { repl_error(S, E_TM); }

static void skipvar(Session *const S) { S->comp.p = comp_skipname(S->comp.p); }

/* cm_name with g_vfs: the M_VAR at comp.p as comp_var(shape, 0). */
static uint16_t shaped(Session *const S, const uint8_t shape)
{
    if (comp_cur(&S->comp) != M_VAR)
        synerr(S);
    return comp_var(S, shape, 0);
}

/* ct_skip: to the statement's end: 0, ':' or '. */
static void skip(Session *const S)
{
    while (*S->comp.p && *S->comp.p != ':' && *S->comp.p != TK_REM)
        S->comp.p = decl_item(S->comp.p);
}

/* ct_skip0: past a ( .. ), if there. */
static void skipparen(Session *const S)
{
    if (*S->comp.p != '(')
        return;
    uint32_t n = 0;
    for (;;) {
        const uint8_t c = *S->comp.p;
        if (!c)
            return;
        if (c == '(')
            n++;
        S->comp.p = decl_item(S->comp.p);
        if (c == ')' && !--n)
            return;
    }
}

/* ---- targets and jumps */

/* ct_target: the target in this line at body offset off, mode. */
static uint32_t target(Session *const S, const uint32_t off, const JumpMode mode)
{
    if (S->comp.line == DLINE)
        return (off * 4 + mode) | PDIR;
    return (off + S->comp.line + HDR) * 4 + mode;
}

/* ct_emitjump: target t as a Jump: its line, body offset, mode; NOTARGET line -1. */
static void emitjump(Session *const S, const uint32_t t)
{
    uint32_t line = NOTARGET, off = 0, mode = 0;
    if (t != NOTARGET) {
        mode = t & 3;
        if (t & PDIR) {
            line = DLINE;
            off = (t & ~PDIR) >> 2;
        } else
            line = blok_lineof(S, t >> 2, &off);
    }
    comp_dword(S, line);
    comp_dword(S, off);
    comp_dword(S, mode);
}

/* ct_ent: the entry of the statement at at; null in the direct line. */
static BlockEnt *entry(Session *const S, const uint8_t *const at)
{
    if (S->comp.line == DLINE)
        return nullptr;
    const int32_t i = blok_find(S, S->comp.line, (uint32_t)(at - S->comp.body));
    if (i < 0)
        synerr(S);
    return &S->blok.blocks[i];
}

static uint32_t nextof(const BlockEnt *const e) { return e ? e->next : NOTARGET; }
static uint32_t doneof(const BlockEnt *const e) { return e ? e->done : NOTARGET; }

static void jump(Session *const S, const uint8_t op, const uint32_t t)
{
    comp_op(S, op);
    emitjump(S, t);
}

static void jmp(Session *const S, const uint32_t t) { jump(S, O_JMP, t); }

static void jcond(Session *const S, const uint8_t op, const uint32_t t)
{
    comp_dtest(S);
    if (S->comp.type & T_STR)
        mismatch(S);
    jump(S, S->comp.type == T_FLT ? op + 1 : op, t); /* F.JF, F.JT after the INT's */
}

void ctrl_jf(Session *const S, const uint32_t t) { jcond(S, O_I_JF, t); }
static void jt(Session *const S, const uint32_t t) { jcond(S, O_I_JT, t); }

void ctrl_blockif(Session *const S)
{
    const BlockEnt *const e = entry(S, S->comp.kw);
    S->comp.p++;
    ctrl_jf(S, nextof(e));
}

bool ctrl_else(Session *const S, uint32_t *const t)
{
    const uint8_t *p = S->comp.p;
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
    *t = target(S, (uint32_t)(p + 1 - S->comp.body), TO_STMT);
    return true;
}

void ctrl_forskip(Session *const S) { jump(S, O_FSKIP, doneof(entry(S, S->comp.kw))); }

/* ct_cond: the test after the keyword. */
static void cond(Session *const S)
{
    S->comp.p++;
    expr_compile(S);
}

static void elseif(Session *const S)
{
    const BlockEnt *e = entry(S, S->comp.p);
    jmp(S, doneof(e));
    cond(S);
    if (*S->comp.p != TK_THEN)
        synerr(S);
    S->comp.p++;
    ctrl_jf(S, nextof(e));
}

static void blockelse(Session *const S)
{
    const BlockEnt *e = entry(S, S->comp.p);
    S->comp.p++;
    jmp(S, doneof(e));
}

static void doloop(Session *const S)
{
    const BlockEnt *e = entry(S, S->comp.p);
    S->comp.p++;
    if (*S->comp.p == TK_WHILE) {
        cond(S);
        ctrl_jf(S, doneof(e));
    } else if (*S->comp.p == TK_UNTIL) {
        cond(S);
        jt(S, doneof(e));
    }
}

static void loop(Session *const S)
{
    const BlockEnt *e = entry(S, S->comp.p);
    S->comp.p++;
    if (*S->comp.p == TK_WHILE) { /* back when true */
        cond(S);
        jt(S, nextof(e));
    } else if (*S->comp.p == TK_UNTIL) { /* back when false */
        cond(S);
        ctrl_jf(S, nextof(e));
    } else
        jmp(S, nextof(e));
}

static void whileloop(Session *const S)
{
    const BlockEnt *e = entry(S, S->comp.p);
    cond(S);
    ctrl_jf(S, doneof(e));
}

static void wend(Session *const S)
{
    const BlockEnt *e = entry(S, S->comp.p);
    S->comp.p++;
    jmp(S, nextof(e));
}

/* EXIT FOR, EXIT DO. */
static void exitblock(Session *const S)
{
    const BlockEnt *const e = entry(S, S->comp.p);
    const uint8_t c = S->comp.p[1];
    S->comp.p += 2;
    if (c == TK_FOR)
        comp_op(S, O_FPOP);
    else if (c != TK_DO)
        synerr(S);
    jmp(S, doneof(e));
}

/* ---- SELECT CASE, SWAP */

/* ct_var: the variable #n of a SELECT at depth n ('S' SWAP's), of type T_*. */
static uint16_t selvar(Session *const S, const uint8_t depth, const uint8_t type)
{
    const uint8_t nm[2] = {'#', (uint8_t)('0' + depth)};
    const uint16_t name = symb_find_name(S, nm, 2);
    S->ctrl.seltype = type;
    S->comp.scope = 0;
    S->comp.isstatic = false;
    S->ctrl.selid = symb_find_var(S, name, comp_pick(type, ET_I4, ET_R4, ET_R8, ET_SD),
                          comp_pick(type, VK_INT, VK_FLT, SK_DBL, VK_STR), 0, false);
    return S->ctrl.selid;
}

/* ct_ldop: selid's load. */
static uint8_t ldop(Session *const S) { return comp_pick(S->ctrl.seltype, O_I_LD_V, O_F_LD_V, O_D_LD_V, O_S_LD_V); }

/* ct_stv: store to selid. */
static void stv(Session *const S) { comp_opid(S, comp_pick(S->ctrl.seltype, O_I_ST_V, O_F_ST_V, O_D_ST_V, O_S_ST_V), S->ctrl.selid); }

static void select(Session *const S)
{
    BlockEnt *const e = entry(S, S->comp.p);
    S->comp.p++;
    if (*S->comp.p != TK_CASE)
        synerr(S);
    S->comp.p++;
    expr_compile(S);
    if (S->comp.type == T_DBL)
        comp_tofloat(S);
    uint8_t depth = 0;
    if (e) {
        depth = (uint8_t)e->aux;
        e->aux = (uint16_t)((e->aux & 0xFF) | (S->comp.type | SEL_COMPILED) << 8);
    }
    selvar(S, depth, S->comp.type);
    stv(S);
    jmp(S, nextof(e));
}

/* ct_cmp: the SELECT's value, relation m (DC_*), the expression: an INT. */
static void casecmp(Session *const S, const uint8_t m)
{
    uint8_t *const left = S->comp.out;
    comp_opid(S, ldop(S), S->ctrl.selid);
    uint8_t *right = S->comp.out;
    expr_compile(S);
    if (S->ctrl.seltype & T_STR) {
        if (!(S->comp.type & T_STR))
            mismatch(S);
        comp_opb(S, O_S_CMP, m);
        S->comp.type = T_INT;
        return;
    }
    if (S->comp.type & T_STR)
        mismatch(S);
    if (S->ctrl.seltype == T_INT) {
        if (S->comp.type == T_INT) {
            expr_icmp(S, REL | m, right);
            return;
        }
        right = expr_lfloat(S, left, right);
    }
    comp_tofloat(S);
    expr_fcmp(S, REL | m, right);
}

/* ct_item: IS rel e, e TO e, e. */
static void caseitem(Session *const S)
{
    if (*S->comp.p == TK_IS) {
        static const uint8_t bit[3] = {DC_GT, DC_EQ, DC_LT};
        S->comp.p++;
        uint8_t m = 0;
        for (uint8_t r; (r = (uint8_t)(*S->comp.p - TK_GT)) < 3; S->comp.p++)
            m |= bit[r];
        if (!m)
            synerr(S);
        casecmp(S, m);
        return;
    }
    for (const uint8_t *p = S->comp.p;; p = decl_item(p)) { /* a TO before ',' or the end */
        const uint8_t c = *p;
        if (!c || c == ':' || c == ',') {
            casecmp(S, DC_EQ);
            return;
        }
        if (c == TK_TO)
            break;
    }
    casecmp(S, DC_GT | DC_EQ);
    if (*S->comp.p != TK_TO)
        synerr(S);
    S->comp.p++;
    casecmp(S, DC_EQ | DC_LT);
    comp_op(S, O_I_AND);
}

static void casestmt(Session *const S)
{
    const BlockEnt *const e = entry(S, S->comp.p);
    jmp(S, doneof(e));
    S->comp.p++;
    if (*S->comp.p == TK_ELSE) {
        S->comp.p++;
        return;
    }
    uint8_t depth = 0, type = T_FLT;
    if (e) { /* its SELECT's depth, type */
        const uint16_t aux = S->blok.blocks[e->aux].aux;
        depth = (uint8_t)aux;
        type = (uint8_t)(aux >> 8);
        if (!type)
            synerr(S); /* its SELECT not compiled */
        type &= (uint8_t)~SEL_COMPILED;
    }
    selvar(S, depth, type);
    caseitem(S);
    while (*S->comp.p == ',') {
        S->comp.p++;
        caseitem(S);
        comp_op(S, O_I_OR);
    }
    S->comp.type = T_INT;
    ctrl_jf(S, nextof(e));
}

void ctrl_swap(Session *const S)
{
    const uint8_t *const a = S->comp.p;
    expr_load(S);
    selvar(S, 'S' - '0', S->comp.type);
    stv(S);
    if (comp_cur(&S->comp) != ',')
        synerr(S);
    const uint8_t *const b = ++S->comp.p;
    S->comp.p = a; /* a = b */
    expr_target(S);
    S->comp.p = b;
    const uint16_t tgt = S->comp.tgt;
    expr_load(S);
    if (S->comp.type != S->ctrl.seltype)
        mismatch(S);
    S->comp.tgt = tgt;
    comp_store(S);
    const uint8_t *const end = S->comp.p;
    S->comp.p = b; /* b = #S */
    expr_target(S);
    comp_opid(S, ldop(S), S->ctrl.selid);
    S->comp.type = S->ctrl.seltype;
    comp_store(S);
    S->comp.p = end;
}

/* ---- records */

/* rc_nrec, rc_nreca: the TYPE of record name, of its array; 0 none. A parameter of the procedure
   compiled is its own, else as DIM declared the name. */
static uint8_t nrec(Session *const S, const uint16_t name, const bool array)
{
    uint16_t id;
    if (comp_param(S, name, VI_ARR, &id)) { /* a record parameter, either kind */
        const uint8_t t = symb_var(&S->symb, id)->type;
        return t >= ET_REC ? t - ET_REC : 0;
    }
    if (!array && comp_param(S, name, 0, &id))
        return 0; /* a parameter, not a record */
    return scop_rec(S, name, array);
}

static uint16_t recvar(Session *const S, const uint16_t name, const uint8_t t, const uint8_t kind)
{
    comp_scope(S, name);
    return symb_find_var(S, name, ET_REC + t, kind, S->comp.scope, S->comp.isstatic);
}

uint16_t ctrl_recarray(Session *const S, const uint16_t name, const uint8_t t) { return recvar(S, name, t, VI_ARR); }

/* rc_whole: all of a record of TYPE t. */
static void whole(Session *const S, const uint8_t t)
{
    S->ctrl.ref.off = 0;
    S->ctrl.ref.et = ET_REC + t;
    S->ctrl.ref.fix = S->decl.types[t].size;
}

/* rc_path: the fields f.g.. of TYPE t, the text len long. */
static void path(Session *const S, uint8_t t, const uint8_t *text, uint32_t len)
{
    S->ctrl.ref.off = 0;
    if (text[len - 1] < '.') /* a suffix is not the name's */
        len--;
    const Field *f;
    for (;;) {
        uint32_t n = 0;
        while (n < len && text[n] != '.')
            n++;
        const uint16_t name = symb_find_name(S, text, (uint8_t)n);
        const TypeDef *const td = &S->decl.types[t];
        f = nullptr;
        for (uint32_t i = 0; i < td->nfld && !f; i++)
            if (S->decl.fields[td->first + i].name == name)
                f = &S->decl.fields[td->first + i];
        if (!f)
            synerr(S);
        S->ctrl.ref.off += f->off;
        text += n;
        len -= n;
        if (!len)
            break;
        if (!f->sub) /* .g: a field of a TYPE's */
            synerr(S);
        t = f->sub;
        text++;
        len--;
    }
    if (f->sub) { /* a record in it */
        const uint16_t off = S->ctrl.ref.off;
        whole(S, f->sub);
        S->ctrl.ref.off = off;
        return;
    }
    S->ctrl.ref.et = f->et;
    S->ctrl.ref.fix = f->fix;
}

bool ctrl_recref(Session *const S)
{
    const uint8_t *const p = S->comp.p;
    const uint8_t len = p[1];
    const uint16_t name = comp_nameof(p);
    const uint8_t *const after = comp_skipname(p);
    uint8_t t;
    if (*after == '(') { /* a(i) */
        t = nrec(S, name, true);
        if (!t)
            return false;
        S->comp.p = after;
        const uint16_t id = ctrl_recarray(S, name, t);
        const uint8_t n = expr_subs(S);
        comp_opw(S, O_RA_ADDR, id);
        comp_byte(S, n);
        if (S->comp.p[0] == '.' && S->comp.p[1] == M_VAR) { /* .f */
            S->comp.p++;
            path(S, t, S->comp.p + 2, S->comp.p[1]);
            S->comp.p = decl_item(S->comp.p);
        } else
            whole(S, t);
        return true;
    }
    t = nrec(S, name, false);
    if (t) { /* r */
        S->comp.p = after;
        comp_opid(S, O_R_ADDR, recvar(S, name, t, VI_RECARR));
        whole(S, t);
        return true;
    }
    const uint8_t *const text = p + 2; /* r.f: the longest prefix a record's */
    for (uint32_t i = len - 1; (int32_t)i > 0; i--) {
        if (text[i] != '.')
            continue;
        const uint16_t pre = symb_find_name(S, text, (uint8_t)i);
        t = nrec(S, pre, false);
        if (!t)
            continue;
        comp_opid(S, O_R_ADDR, recvar(S, pre, t, VI_RECARR));
        path(S, t, text + i + 1, len - i - 1);
        S->comp.p = after;
        return true;
    }
    return false;
}

void ctrl_recload(Session *const S)
{
    const uint8_t et = S->ctrl.ref.et;
    if (et >= ET_REC)
        mismatch(S);
    if (et == ET_SD) {
        comp_opw(S, O_SF_LD, S->ctrl.ref.off);
        comp_word(S, S->ctrl.ref.fix);
        S->comp.type = T_STR;
    } else {
        comp_opw(S, O_FL_LD, S->ctrl.ref.off);
        comp_byte(S, et);
        if (et == ET_SH) {
            S->expr.sh = SH_SHORT;
            S->expr.shend = S->comp.out;
        }
        S->comp.type = expr_ktype[comp_etkind[et]];
    }
    S->comp.iload = S->comp.iloadend = nullptr;
}

void ctrl_recstore(Session *const S)
{
    const uint8_t et = S->ctrl.fieldet;
    if (et == ET_SD) {
        comp_tostring(S);
        comp_opw(S, O_SF_ST, S->comp.tgt);
        comp_word(S, S->ctrl.fieldfix);
        return;
    }
    comp_toinfo(S, comp_etkind[et]);
    comp_opw(S, O_FL_ST, S->comp.tgt);
    comp_byte(S, et);
}

/* ct_rcopy: a = b of two records of one TYPE: their addresses, R.COPY. */
static bool reccopy(Session *const S)
{
    const uint8_t *const p = S->comp.p;
    uint8_t *const out = S->comp.out;
    if (ctrl_recref(S) && S->ctrl.ref.et >= ET_REC) {
        const uint8_t et = S->ctrl.ref.et;
        if (comp_cur(&S->comp) == TK_EQ) {
            S->comp.p++;
            if (comp_cur(&S->comp) == M_VAR && ctrl_recref(S) && S->ctrl.ref.et == et && comp_end(S)) {
                comp_opw(S, O_R_COPY, S->ctrl.ref.fix);
                return true;
            }
        }
    }
    S->comp.p = p;
    S->comp.out = out;
    return false;
}

/* ---- procedures */

/* ct_proc: the procedure the name at comp.p is, its index to *idx, comp.p past the name; null
   none, comp.p kept. */
static const Proc *proc(Session *const S, uint8_t *const idx)
{
    const uint8_t *const p = S->comp.p;
    if (*p != M_VAR)
        return nullptr;
    const Name *const n = symb_name(&S->symb, comp_nameof(p));
    if (!(n->flags & (NM_SUB | NM_FUNC | NM_FN)) || !n->proc)
        return nullptr;
    S->comp.p = comp_skipname(p);
    *idx = n->proc;
    return &S->blok.procs[n->proc];
}

/* ct_pdecl: the name at comp.p a SUB's or FUNCTION's that a DECLARE or a definition named. */
static bool declname(Session *const S)
{
    return *S->comp.p == M_VAR &&
           symb_name(&S->symb, comp_nameof(S->comp.p))->flags & (NM_SUB | NM_FUNC | NM_FN);
}

/* ct_pchk: in the IDE's check of a line alone, a procedure its declaration named passes:
   blok_procs has it only once linked. */
static bool checkonly(Session *const S) { return S->comp.check && declname(S); }

void ctrl_undef(Session *const S, const uint8_t err)
{
    comp_int(S, err);
    comp_call(S, K_ERROR);
}

void ctrl_dropargs(Session *const S, const bool paren)
{
    uint8_t *const out = S->comp.out;
    for (;;) {
        if (paren ? *S->comp.p == ')' : comp_end(S))
            break;
        if (*S->comp.p == TK_BYVAL)
            S->comp.p++;
        const uint8_t *const p = S->comp.p;
        const uint8_t *const after = *p == M_VAR ? comp_skipname(p) : p;
        if (*p == M_VAR && after[0] == '(' && after[1] == ')')
            S->comp.p = after + 2; /* a() an array whole */
        else
            expr_compile(S);
        if (*S->comp.p != ',')
            break;
        S->comp.p++;
    }
    S->comp.out = out;
}

/* ct_aend: c ends an argument. */
static bool aend(const uint8_t c) { return !c || c == ',' || c == ')' || c == ':' || c == TK_REM; }

/* ct_ptm: variable id the parameter's type; else Parameter type mismatch. */
static void ptm(Session *const S, const uint16_t id)
{
    if (symb_var(&S->symb, id)->type != symb_var(&S->symb, S->ctrl.param)->type)
        repl_error(S, E_PTM);
}

/* An array parameter's argument: n() by its id, twice (its ref: PRET's store back); a record
   parameter's n by its id, FFFFh. */
static void arrayarg(Session *const S)
{
    if (comp_cur(&S->comp) != M_VAR)
        synerr(S);
    const uint16_t name = comp_nameof(S->comp.p);
    const uint8_t *const after = comp_skipname(S->comp.p);
    if (*after == '(') {
        const uint8_t t = scop_rec(S, name, true);
        uint16_t id;
        if (t) {
            S->comp.p = after;
            id = ctrl_recarray(S, name, t);
        } else
            id = comp_var(S, 0, 0);
        if (!(symb_var(&S->symb, id)->info & VI_ARR))
            synerr(S);
        ptm(S, id);
        comp_open(S);
        comp_close(S);
        comp_int(S, id);
        comp_int(S, id);
        return;
    }
    const uint8_t t = scop_rec(S, name, false);
    if (!t)
        synerr(S);
    S->comp.p = after;
    comp_int(S, recvar(S, name, t, VI_RECARR));
    comp_int(S, 0xFFFF);
}

/* ct_arg: an argument to parameter par: a variable alone by reference (V.ADDR), an element or a
   record's number field by P.REF, else the value and FFFFh. A CONST, BYVAL's, a DEF FN's: the
   value. */
static void arg(Session *const S, const uint16_t par)
{
    S->ctrl.param = par;
    const uint8_t info = symb_var(&S->symb, par)->info;
    if (info & VI_ARR) {
        arrayarg(S);
        return;
    }
    if (!S->ctrl.byval && comp_cur(&S->comp) == M_VAR) {
        const uint16_t name = comp_nameof(S->comp.p);
        if (!(symb_name(&S->symb, name)->flags & (NM_SUB | NM_FUNC | NM_FN)) && !scop_con(S, name)) {
            const uint8_t *const p = S->comp.p;
            const uint8_t *const after = comp_skipname(p);
            uint8_t *const out = S->comp.out;
            const uint8_t ptype = symb_var(&S->symb, par)->type;
            if (ctrl_recref(S)) { /* a field */
                const uint8_t et = S->ctrl.ref.et;
                if (!(et == ET_SD && S->ctrl.ref.fix) && et != ET_SH && et < ET_REC && et == ptype &&
                    aend(comp_cur(&S->comp))) {
                    comp_opw(S, O_P_REF, S->ctrl.ref.off);
                    return;
                }
            } else if (*after == '(') { /* an element */
                const uint16_t id = comp_var(S, 0, 0);
                const Var *const v = symb_var(&S->symb, id);
                if (v->info & VI_ARR && !scop_fix(S, v->name, true) && v->type == ptype) {
                    const uint8_t n = expr_subs(S);
                    comp_opw(S, O_RA_ADDR, id);
                    comp_byte(S, n);
                    if (aend(comp_cur(&S->comp))) {
                        comp_opw(S, O_P_REF, 0);
                        return;
                    }
                }
            }
            S->comp.p = p;
            S->comp.out = out;
            if (aend(*after)) { /* a variable alone: its cell */
                const uint16_t id = comp_var(S, 0, 0);
                ptm(S, id);
                comp_int(S, 0);
                comp_opid(S, O_V_ADDR, id);
                return;
            }
        }
    }
    expr_compile(S);
    comp_toinfo(S, info);
    comp_int(S, 0xFFFF);
}

/* ct_args: for each of the procedure's parameters, its argument. */
static void args(Session *const S, const Proc *const pr)
{
    const bool def = pr->flags & PF_DEF; /* a DEF FN's: by value */
    for (uint32_t i = 0; i < pr->npar; i++) {
        if (i) {
            if (comp_cur(&S->comp) != ',')
                synerr(S);
            S->comp.p++;
        }
        S->ctrl.byval = S->blok.byval[pr->par + i] || def;
        arg(S, S->blok.params[pr->par + i]);
    }
}

/* ( args ), if there. */
static void argsparen(Session *const S, const Proc *const pr)
{
    if (*S->comp.p != '(')
        return;
    S->comp.p++;
    args(S, pr);
    comp_close(S);
}

static void pcall(Session *const S, const uint8_t idx, const Proc *const pr)
{
    comp_opb(S, O_PCALL, idx);
    emitjump(S, pr->entry);
}

void ctrl_fcall(Session *const S)
{
    uint8_t idx;
    const Proc *const pr = proc(S, &idx);
    if (!pr)
        synerr(S);
    argsparen(S, pr);
    pcall(S, idx, pr);
    S->comp.type = expr_ktype[symb_var(&S->symb, pr->result)->info & (SK_DBL | VK_MASK)];
    S->comp.iload = S->comp.iloadend = nullptr;
}

/* CALL n [( args )]. */
static void call(Session *const S)
{
    S->comp.p++;
    uint8_t idx;
    const Proc *const pr = proc(S, &idx);
    if (!pr) {
        if (!declname(S))
            synerr(S);
        if (S->comp.check) {
            skip(S);
            return;
        }
        skipvar(S); /* DECLAREd only: its arguments, then Subprogram not defined when run */
        if (*S->comp.p == '(') {
            S->comp.p++;
            ctrl_dropargs(S, true);
            comp_close(S);
        }
        ctrl_undef(S, QB_NO_SUB);
        return;
    }
    argsparen(S, pr);
    pcall(S, idx, pr);
}

/* ct_scall: n args, a SUB's call without CALL; another name is not one. */
static bool subcall(Session *const S)
{
    const uint8_t *const start = S->comp.p;
    uint8_t idx;
    const Proc *const pr = proc(S, &idx);
    if (pr) {
        if (pr->kind == PK_SUB) {
            if (!comp_end(S)) {
                if (comp_cur(&S->comp) == TK_EQ) /* a SUB's name assigned */
                    synerr(S);
                args(S, pr);
            }
            pcall(S, idx, pr);
            return true;
        }
        if (idx != S->comp.proc && comp_cur(&S->comp) == TK_EQ) /* f = e: in FUNCTION f only */
            repl_error(S, E_VR);
    }
    S->comp.p = start;
    if (checkonly(S)) {
        skip(S);
        return true;
    }
    if (*S->comp.p != M_VAR)
        return false;
    const Name *const n = symb_name(&S->symb, comp_nameof(S->comp.p));
    if (!(n->flags & NM_SUB) || n->proc)
        return false;
    skipvar(S); /* a SUB DECLAREd only */
    if (!comp_end(S)) {
        if (comp_cur(&S->comp) == TK_EQ)
            synerr(S);
        ctrl_dropargs(S, false);
    }
    ctrl_undef(S, QB_NO_SUB);
    return true;
}

/* SUB, FUNCTION: run into, passed: JMP past its END. */
static void sub(Session *const S)
{
    S->comp.p++;
    uint8_t idx;
    const Proc *const pr = proc(S, &idx);
    if (pr)
        jmp(S, pr->skip);
    else if (!checkonly(S))
        synerr(S);
    skip(S);
}

/* END or EXIT SUB, FUNCTION, DEF: PRET. */
static void pret(Session *const S)
{
    S->comp.p += 2;
    comp_op(S, O_PRET);
}

/* DEF FNn [(params)]: passed as a FUNCTION; with = e, JMP past it, then the result e, PRET. */
static void deffn(Session *const S)
{
    const uint8_t *const name = ++S->comp.p;
    uint8_t idx;
    const Proc *const pr = proc(S, &idx);
    if (!pr) {
        if (!checkonly(S))
            synerr(S);
        skip(S);
        return;
    }
    jmp(S, pr->skip);
    skipparen(S);
    if (comp_cur(&S->comp) != TK_EQ) {
        skip(S);
        return;
    }
    const uint8_t *const eq = S->comp.p;
    S->comp.p = name;
    comp_target(S, shaped(S, 1)); /* its result */
    S->comp.p = eq + 1;
    expr_compile(S);
    comp_store(S);
    comp_op(S, O_PRET);
}

/* DEF SEG [= s]: no code, flat; DEF FN. */
static bool defseg(Session *const S)
{
    if (S->comp.p[1] == TK_X && S->comp.p[2] == TK1_SEG) {
        skip(S);
        return true;
    }
    if (S->comp.p[1] != M_VAR)
        return false;
    deffn(S);
    return true;
}

/* ---- other statements */

/* REDIM [PRESERVE]: DIM's grammar, each CALL DIM marked REDIM's. */
static void redim(Session *const S)
{
    S->comp.p++;
    S->comp.redim = DIM_REDIM;
    if (S->comp.p[0] == TK_X && S->comp.p[1] == TK1_PRESERVE) {
        S->comp.p += 2;
        S->comp.redim = DIM_REDIM | DIM_PRESERVE;
    }
    comp_dfa(S, lexr_ps_entry[TK_DIM - 0x80]);
    S->comp.redim = 0;
}

/* ERASE a {, a}: each array's id, CALL ERASE. */
static void erase(Session *const S)
{
    S->comp.p++;
    do {
        uint16_t id = shaped(S, 2);
        const uint16_t name = symb_var(&S->symb, id)->name;
        const uint8_t t = scop_rec(S, name, true); /* a record array: its own variable */
        if (t)
            id = ctrl_recarray(S, name, t);
        comp_int(S, id);
        comp_call(S, K_ERASE);
    } while (comp_optcomma(S));
}

/* ct_evk: the TK_X word at comp.p an event's: its EK_*, comp.p past it. */
static bool evkind(Session *const S, uint8_t *const kind)
{
    switch (S->comp.p[1]) {
    case TK1_TIMER: *kind = EK_TIMER; break;
    case TK1_KEY: *kind = EK_KEY; break;
    case TK1_COM: *kind = EK_COM; break;
    case TK1_PEN: *kind = EK_PEN; break;
    case TK1_STRIG: *kind = EK_STRIG; break;
    default: return false;
    }
    S->comp.p += 2;
    return true;
}

/* ct_evn: the event's (n): KEY's, COM's, STRIG's; TIMER's in ON TIMER; else 0. */
static void evnum(Session *const S, const uint8_t kind)
{
    if (kind != EK_PEN) {
        if (comp_cur(&S->comp) == '(') {
            expr_paren(S);
            comp_toint(S);
            return;
        }
        if (kind != EK_TIMER)
            synerr(S);
    }
    comp_int(S, 0);
}

/* ON [LOCAL] ERROR GOTO line: ONERR or ONLERR num addr; ON event(n) GOSUB line: ONEV kind num
   addr; another ON is not one. */
static bool onerror(Session *const S)
{
    const uint8_t *const start = S->comp.p++;
    if (comp_cur(&S->comp) != TK_X) {
        S->comp.p = start;
        return false;
    }
    uint8_t kind;
    if (evkind(S, &kind)) {
        evnum(S, kind);
        if (comp_cur(&S->comp) != TK_GOSUB)
            synerr(S);
        S->comp.p++;
        S->comp.modlabel = true; /* an event's handler: the module's */
        comp_lnum(S);
        comp_opb(S, O_ONEV, kind);
        comp_lref(S);
        return true;
    }
    uint8_t op = O_ONERR;
    if (S->comp.p[1] == TK1_LOCAL) { /* PDS's ON LOCAL ERROR */
        S->comp.p += 2;
        if (comp_cur(&S->comp) != TK_X)
            synerr(S);
        op = O_ONLERR;
    }
    if (S->comp.p[1] != TK1_ERROR) {
        S->comp.p = start;
        return false;
    }
    S->comp.p += 2;
    if (comp_cur(&S->comp) != TK_GOTO) {
        S->comp.dg = DG_GOTO_MISSING;
        synerr(S);
    }
    S->comp.p++;
    S->comp.modlabel = op == O_ONERR; /* ON ERROR's handler: the module's */
    comp_lnum(S);
    comp_op(S, op);
    comp_lref(S);
    return true;
}

/* ct_evs: event(n) ON | OFF | STOP: CALL EVST ( n kind*4+state -- ); KEY n, s: CALL KEYDEF; KEY
   ON, OFF, LIST: the screen's, nothing. */
static bool evstate(Session *const S)
{
    uint8_t kind;
    if (!evkind(S, &kind))
        return false;
    if (kind == EK_KEY) {
        const uint8_t c = comp_cur(&S->comp);
        if (c == TK_ON || c == TK_LIST) {
            S->comp.p++;
            return true;
        }
        if (c == TK_X && S->comp.p[1] == TK1_OFF) {
            S->comp.p += 2;
            return true;
        }
        if (c != '(') {
            expr_compile(S);
            comp_toint(S);
            comp_comma(S);
            expr_compile(S);
            comp_tostring(S);
            comp_call(S, K_KEYDEF);
            return true;
        }
    }
    evnum(S, kind);
    const uint8_t c = comp_cur(&S->comp);
    uint8_t state;
    if (c == TK_X) {
        if (S->comp.p[1] != TK1_OFF)
            synerr(S);
        S->comp.p++;
        state = ES_OFF;
    } else if (c == TK_ON)
        state = ES_ON;
    else if (c == TK_STOP)
        state = ES_STOP;
    else
        synerr(S);
    S->comp.p++;
    comp_int(S, kind * 4u + state);
    comp_call(S, K_EVST);
    return true;
}

/* COMMON [SHARED] [/block/] v[()] [AS type] {, ..}: each variable's id, CALL CMN. */
static void common(Session *const S)
{
    S->comp.p += 2;
    if (comp_cur(&S->comp) == TK_SHARED)
        S->comp.p++;
    if (comp_cur(&S->comp) == TK_DIV) {
        S->comp.p++;
        if (comp_cur(&S->comp) != M_VAR)
            synerr(S);
        skipvar(S);
        if (comp_cur(&S->comp) != TK_DIV)
            synerr(S);
        S->comp.p++;
    }
    do {
        const uint16_t id = comp_name(S);
        if (comp_cur(&S->comp) == '(') {
            S->comp.p++;
            if (comp_cur(&S->comp) != ')')
                synerr(S);
            S->comp.p++;
        }
        if (comp_cur(&S->comp) == TK_AS) { /* AS type [* n] */
            S->comp.p++;
            const uint8_t c = comp_cur(&S->comp);
            if (c == M_VAR)
                skipvar(S);
            else if (c == TK_X)
                S->comp.p += 2;
            else {
                S->comp.p++;
                if (comp_cur(&S->comp) == TK_MUL) {
                    S->comp.p++;
                    if (comp_cur(&S->comp) != M_INT)
                        synerr(S);
                    S->comp.p += INT_TOKEN_SIZE;
                }
            }
        }
        comp_int(S, id);
        comp_call(S, K_CMN);
    } while (comp_optcomma(S));
}

/* RESUME [NEXT | 0 | line]: RESUME mode num addr. */
static void resume(Session *const S)
{
    S->comp.p += 2;
    S->comp.lnum = 0;
    S->comp.jline = 0;
    ResumeMode mode = RESUME_AGAIN;
    if (comp_cur(&S->comp) == TK_NEXT) {
        S->comp.p++;
        mode = RESUME_NEXT;
    } else if (!comp_end(S)) {
        comp_lnum(S);
        if (S->comp.lnum || S->comp.jline) /* not RESUME 0 */
            mode = RESUME_LINE;
    }
    comp_opb(S, O_RESUME, mode);
    comp_lref(S);
}

static bool xstmt(Session *const S)
{
    switch (S->comp.p[1]) {
    case TK1_COMMON: common(S); return true;
    case TK1_DEFSHORT: skip(S); return true;
    case TK1_RESUME: resume(S); return true;
    }
    return evstate(S);
}

/* MID$(v, i [, n]) = s: v = its value with s over it. */
static void mids(Session *const S)
{
    S->comp.p++;
    comp_open(S);
    const uint8_t *const v = S->comp.p;
    expr_target(S);
    if ((S->comp.tgtinfo & VK_MASK) != VK_STR)
        mismatch(S);
    const uint16_t tgt = S->comp.tgt;
    S->comp.p = v;
    expr_load(S);
    comp_comma(S);
    expr_compile(S);
    comp_toint(S);
    if (comp_optcomma(S)) {
        expr_compile(S);
        comp_toint(S);
    } else
        comp_int(S, SMAX);
    comp_close(S);
    comp_equals(S);
    expr_compile(S);
    comp_tostring(S);
    comp_call(S, K_MIDS);
    S->comp.tgt = tgt;
    S->comp.type = T_STR;
    comp_store(S);
}

/* A statement starting with a name: a TYPE's field, a record's copy, a SUB's call. */
static bool namestmt(Session *const S)
{
    if (*comp_skipname(S->comp.p) == TK_AS) {
        skip(S);
        return true;
    }
    return reccopy(S) || subcall(S);
}

/* LET: a record's copy, else not one. */
static bool let(Session *const S)
{
    S->comp.p++;
    if (*S->comp.p == M_VAR && reccopy(S))
        return true;
    S->comp.p--;
    return false;
}

bool ctrl_stmt(Session *const S)
{
    const uint8_t c = S->comp.p[0];
    const uint8_t n = S->comp.p[1];
    switch (c) {
    case TK_ELSEIF: elseif(S); return true;
    case TK_ELSE: blockelse(S); return true;
    case TK_ENDIF: S->comp.p++; return true;
    case TK_SELECT: select(S); return true;
    case TK_CASE: casestmt(S); return true;
    case TK_DO: doloop(S); return true;
    case TK_LOOP: loop(S); return true;
    case TK_WHILE: whileloop(S); return true;
    case TK_WEND: wend(S); return true;
    case M_VAR: return namestmt(S);
    case TK_LET: return let(S);
    case TK_CALL: call(S); return true;
    case TK_SUB:
    case TK_FUNCTION: sub(S); return true;
    case TK_DEF: return defseg(S);
    case TK_MID: mids(S); return true;
    case TK_ON: return onerror(S);
    case TK_REDIM: redim(S); return true;
    case TK_ERASE: erase(S); return true;
    case TK_X: return xstmt(S);
    case TK_TYPE:
    case TK_DECLARE:
    case TK_OPTION:
    case TK_SHARED:
    case TK_STATIC: skip(S); return true; /* entered with the line (decl_line), no code */
    case TK_EXIT:
        if (n == TK_SUB || n == TK_FUNCTION || n == TK_DEF)
            pret(S);
        else
            exitblock(S);
        return true;
    case TK_END:
        if (n == TK_IF || n == TK_SELECT || n == TK_TYPE) { /* no code */
            S->comp.p += 2;
            return true;
        }
        if (n == TK_SUB || n == TK_FUNCTION || n == TK_DEF) {
            pret(S);
            return true;
        }
    }
    return false;
}
