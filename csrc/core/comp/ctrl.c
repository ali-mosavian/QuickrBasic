/* Block statements, procedures' calls, records' references (comp/ctl.asm). */
#include "comp/ctrl.h"
#include "comp/blok.h"
#include "comp/decl.h"
#include "comp/dfa.h"
#include "comp/expr.h"
#include "io/msg.h"
#include "io/repl.h"
#include "mem/scop.h"
#include "mem/symb.h"
#include "num/dubl.h"

Ctrl ctrl;

/* A SELECT's entry's aux, high byte: its value's T_* | SEL_COMPILED (T_INT is 0). */
enum : uint8_t { SEL_COMPILED = 1 };

static uint16_t selid;  /* g_ctvr: a SELECT's or SWAP's variable */
static uint8_t seltype; /* g_cttp: its T_* */
static bool byval;      /* g_cbyv: the argument by value */
static uint16_t param;  /* g_cpar: arg's parameter */

[[noreturn]] static void synerr(void) { repl_error(E_SN); }
[[noreturn]] static void mismatch(void) { repl_error(E_TM); }

static void skipvar(void) { comp.p = comp_skipname(comp.p); }

/* cm_name with g_vfs: the M_VAR at comp.p as comp_var(shape, 0). */
static uint16_t shaped(const uint8_t shape)
{
    if (comp_cur() != M_VAR)
        synerr();
    return comp_var(shape, 0);
}

/* ct_skip: to the statement's end: 0, ':' or '. */
static void skip(void)
{
    while (*comp.p && *comp.p != ':' && *comp.p != TK_REM)
        comp.p = decl_item(comp.p);
}

/* ct_skip0: past a ( .. ), if there. */
static void skipparen(void)
{
    if (*comp.p != '(')
        return;
    uint32_t n = 0;
    for (;;) {
        const uint8_t c = *comp.p;
        if (!c)
            return;
        if (c == '(')
            n++;
        comp.p = decl_item(comp.p);
        if (c == ')' && !--n)
            return;
    }
}

/* ---- targets and jumps */

/* ct_target: the target in this line at body offset off, mode. */
static uint32_t target(const uint32_t off, const JumpMode mode)
{
    if (comp.line == DLINE)
        return (off * 4 + mode) | PDIR;
    return (off + comp.line + HDR) * 4 + mode;
}

/* ct_emitjump: target t as a Jump: its line, body offset, mode; NOTARGET line -1. */
static void emitjump(const uint32_t t)
{
    uint32_t line = NOTARGET, off = 0, mode = 0;
    if (t != NOTARGET) {
        mode = t & 3;
        if (t & PDIR) {
            line = DLINE;
            off = (t & ~PDIR) >> 2;
        } else
            line = blok_lineof(t >> 2, &off);
    }
    comp_dword(line);
    comp_dword(off);
    comp_dword(mode);
}

/* ct_ent: the entry of the statement at at; null in the direct line. */
static BlockEnt *entry(const uint8_t *const at)
{
    if (comp.line == DLINE)
        return nullptr;
    const int32_t i = blok_find(comp.line, (uint32_t)(at - comp.body));
    if (i < 0)
        synerr();
    return &blok_blocks[i];
}

static uint32_t nextof(const BlockEnt *const e) { return e ? e->next : NOTARGET; }
static uint32_t doneof(const BlockEnt *const e) { return e ? e->done : NOTARGET; }

static void jump(const uint8_t op, const uint32_t t)
{
    comp_op(op);
    emitjump(t);
}

static void jmp(const uint32_t t) { jump(O_JMP, t); }

static void jcond(const uint8_t op, const uint32_t t)
{
    comp_dtest();
    if (comp.type & T_STR)
        mismatch();
    jump(comp.type == T_FLT ? op + 1 : op, t); /* F.JF, F.JT after the INT's */
}

void ctrl_jf(const uint32_t t) { jcond(O_I_JF, t); }
static void jt(const uint32_t t) { jcond(O_I_JT, t); }

void ctrl_blockif(void)
{
    const BlockEnt *const e = entry(comp.kw);
    comp.p++;
    ctrl_jf(nextof(e));
}

bool ctrl_else(uint32_t *const t)
{
    const uint8_t *p = comp.p;
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
    *t = target((uint32_t)(p + 1 - comp.body), TO_STMT);
    return true;
}

void ctrl_forskip(void) { jump(O_FSKIP, doneof(entry(comp.kw))); }

/* ct_cond: the test after the keyword. */
static void cond(void)
{
    comp.p++;
    expr_compile();
}

static void elseif(void)
{
    const BlockEnt *e = entry(comp.p);
    jmp(doneof(e));
    cond();
    if (*comp.p != TK_THEN)
        synerr();
    comp.p++;
    ctrl_jf(nextof(e));
}

static void blockelse(void)
{
    const BlockEnt *e = entry(comp.p);
    comp.p++;
    jmp(doneof(e));
}

static void doloop(void)
{
    const BlockEnt *e = entry(comp.p);
    comp.p++;
    if (*comp.p == TK_WHILE) {
        cond();
        ctrl_jf(doneof(e));
    } else if (*comp.p == TK_UNTIL) {
        cond();
        jt(doneof(e));
    }
}

static void loop(void)
{
    const BlockEnt *e = entry(comp.p);
    comp.p++;
    if (*comp.p == TK_WHILE) { /* back when true */
        cond();
        jt(nextof(e));
    } else if (*comp.p == TK_UNTIL) { /* back when false */
        cond();
        ctrl_jf(nextof(e));
    } else
        jmp(nextof(e));
}

static void whileloop(void)
{
    const BlockEnt *e = entry(comp.p);
    cond();
    ctrl_jf(doneof(e));
}

static void wend(void)
{
    const BlockEnt *e = entry(comp.p);
    comp.p++;
    jmp(nextof(e));
}

/* EXIT FOR, EXIT DO. */
static void exitblock(void)
{
    const BlockEnt *const e = entry(comp.p);
    const uint8_t c = comp.p[1];
    comp.p += 2;
    if (c == TK_FOR)
        comp_op(O_FPOP);
    else if (c != TK_DO)
        synerr();
    jmp(doneof(e));
}

/* ---- SELECT CASE, SWAP */

/* ct_var: the variable #n of a SELECT at depth n ('S' SWAP's), of type T_*. */
static uint16_t selvar(const uint8_t depth, const uint8_t type)
{
    const uint8_t nm[2] = {'#', (uint8_t)('0' + depth)};
    const uint16_t name = symb_find_name(nm, 2);
    seltype = type;
    comp.scope = 0;
    comp.isstatic = false;
    selid = symb_find_var(name, comp_pick(type, ET_I4, ET_R4, ET_R8, ET_SD),
                          comp_pick(type, VK_INT, VK_FLT, SK_DBL, VK_STR), 0, false);
    return selid;
}

/* ct_ldop: selid's load. */
static uint8_t ldop(void) { return comp_pick(seltype, O_I_LD_V, O_F_LD_V, O_D_LD_V, O_S_LD_V); }

/* ct_stv: store to selid. */
static void stv(void) { comp_opid(comp_pick(seltype, O_I_ST_V, O_F_ST_V, O_D_ST_V, O_S_ST_V), selid); }

static void select(void)
{
    BlockEnt *const e = entry(comp.p);
    comp.p++;
    if (*comp.p != TK_CASE)
        synerr();
    comp.p++;
    expr_compile();
    if (comp.type == T_DBL)
        comp_tofloat();
    uint8_t depth = 0;
    if (e) {
        depth = (uint8_t)e->aux;
        e->aux = (uint16_t)((e->aux & 0xFF) | (comp.type | SEL_COMPILED) << 8);
    }
    selvar(depth, comp.type);
    stv();
    jmp(nextof(e));
}

/* ct_cmp: the SELECT's value, relation m (DC_*), the expression: an INT. */
static void casecmp(const uint8_t m)
{
    uint8_t *const left = comp.out;
    comp_opid(ldop(), selid);
    uint8_t *right = comp.out;
    expr_compile();
    if (seltype & T_STR) {
        if (!(comp.type & T_STR))
            mismatch();
        comp_opb(O_S_CMP, m);
        comp.type = T_INT;
        return;
    }
    if (comp.type & T_STR)
        mismatch();
    if (seltype == T_INT) {
        if (comp.type == T_INT) {
            expr_icmp(REL | m, right);
            return;
        }
        right = expr_lfloat(left, right);
    }
    comp_tofloat();
    expr_fcmp(REL | m, right);
}

/* ct_item: IS rel e, e TO e, e. */
static void caseitem(void)
{
    if (*comp.p == TK_IS) {
        static const uint8_t bit[3] = {DC_GT, DC_EQ, DC_LT};
        comp.p++;
        uint8_t m = 0;
        for (uint8_t r; (r = (uint8_t)(*comp.p - TK_GT)) < 3; comp.p++)
            m |= bit[r];
        if (!m)
            synerr();
        casecmp(m);
        return;
    }
    for (const uint8_t *p = comp.p;; p = decl_item(p)) { /* a TO before ',' or the end */
        const uint8_t c = *p;
        if (!c || c == ':' || c == ',') {
            casecmp(DC_EQ);
            return;
        }
        if (c == TK_TO)
            break;
    }
    casecmp(DC_GT | DC_EQ);
    if (*comp.p != TK_TO)
        synerr();
    comp.p++;
    casecmp(DC_EQ | DC_LT);
    comp_op(O_I_AND);
}

static void casestmt(void)
{
    const BlockEnt *const e = entry(comp.p);
    jmp(doneof(e));
    comp.p++;
    if (*comp.p == TK_ELSE) {
        comp.p++;
        return;
    }
    uint8_t depth = 0, type = T_FLT;
    if (e) { /* its SELECT's depth, type */
        const uint16_t aux = blok_blocks[e->aux].aux;
        depth = (uint8_t)aux;
        type = (uint8_t)(aux >> 8);
        if (!type)
            synerr(); /* its SELECT not compiled */
        type &= (uint8_t)~SEL_COMPILED;
    }
    selvar(depth, type);
    caseitem();
    while (*comp.p == ',') {
        comp.p++;
        caseitem();
        comp_op(O_I_OR);
    }
    comp.type = T_INT;
    ctrl_jf(nextof(e));
}

void ctrl_swap(void)
{
    const uint8_t *const a = comp.p;
    expr_load();
    selvar('S' - '0', comp.type);
    stv();
    if (comp_cur() != ',')
        synerr();
    const uint8_t *const b = ++comp.p;
    comp.p = a; /* a = b */
    expr_target();
    comp.p = b;
    const uint16_t tgt = comp.tgt;
    expr_load();
    if (comp.type != seltype)
        mismatch();
    comp.tgt = tgt;
    comp_store();
    const uint8_t *const end = comp.p;
    comp.p = b; /* b = #S */
    expr_target();
    comp_opid(ldop(), selid);
    comp.type = seltype;
    comp_store();
    comp.p = end;
}

/* ---- records */

/* rc_nrec, rc_nreca: the TYPE of record name, of its array; 0 none. A parameter of the procedure
   compiled is its own, else as DIM declared the name. */
static uint8_t nrec(const uint16_t name, const bool array)
{
    uint16_t id;
    if (comp_param(name, VI_ARR, &id)) { /* a record parameter, either kind */
        const uint8_t t = symb_var(id)->type;
        return t >= ET_REC ? t - ET_REC : 0;
    }
    if (!array && comp_param(name, 0, &id))
        return 0; /* a parameter, not a record */
    return scop_rec(name, array);
}

static uint16_t recvar(const uint16_t name, const uint8_t t, const uint8_t kind)
{
    comp_scope(name);
    return symb_find_var(name, ET_REC + t, kind, comp.scope, comp.isstatic);
}

uint16_t ctrl_recarray(const uint16_t name, const uint8_t t) { return recvar(name, t, VI_ARR); }

/* rc_whole: all of a record of TYPE t. */
static void whole(const uint8_t t)
{
    ctrl.ref.off = 0;
    ctrl.ref.et = ET_REC + t;
    ctrl.ref.fix = decl_types[t].size;
}

/* rc_path: the fields f.g.. of TYPE t, the text len long. */
static void path(uint8_t t, const uint8_t *text, uint32_t len)
{
    ctrl.ref.off = 0;
    if (text[len - 1] < '.') /* a suffix is not the name's */
        len--;
    const Field *f;
    for (;;) {
        uint32_t n = 0;
        while (n < len && text[n] != '.')
            n++;
        const uint16_t name = symb_find_name(text, (uint8_t)n);
        const TypeDef *const td = &decl_types[t];
        f = nullptr;
        for (uint32_t i = 0; i < td->nfld && !f; i++)
            if (decl_fields[td->first + i].name == name)
                f = &decl_fields[td->first + i];
        if (!f)
            synerr();
        ctrl.ref.off += f->off;
        text += n;
        len -= n;
        if (!len)
            break;
        if (!f->sub) /* .g: a field of a TYPE's */
            synerr();
        t = f->sub;
        text++;
        len--;
    }
    if (f->sub) { /* a record in it */
        const uint16_t off = ctrl.ref.off;
        whole(f->sub);
        ctrl.ref.off = off;
        return;
    }
    ctrl.ref.et = f->et;
    ctrl.ref.fix = f->fix;
}

bool ctrl_recref(void)
{
    const uint8_t *const p = comp.p;
    const uint8_t len = p[1];
    const uint16_t name = comp_nameof(p);
    const uint8_t *const after = comp_skipname(p);
    uint8_t t;
    if (*after == '(') { /* a(i) */
        t = nrec(name, true);
        if (!t)
            return false;
        comp.p = after;
        const uint16_t id = ctrl_recarray(name, t);
        const uint8_t n = expr_subs();
        comp_opw(O_RA_ADDR, id);
        comp_byte(n);
        if (comp.p[0] == '.' && comp.p[1] == M_VAR) { /* .f */
            comp.p++;
            path(t, comp.p + 2, comp.p[1]);
            comp.p = decl_item(comp.p);
        } else
            whole(t);
        return true;
    }
    t = nrec(name, false);
    if (t) { /* r */
        comp.p = after;
        comp_opid(O_R_ADDR, recvar(name, t, VI_RECARR));
        whole(t);
        return true;
    }
    const uint8_t *const text = p + 2; /* r.f: the longest prefix a record's */
    for (uint32_t i = len - 1; (int32_t)i > 0; i--) {
        if (text[i] != '.')
            continue;
        const uint16_t pre = symb_find_name(text, (uint8_t)i);
        t = nrec(pre, false);
        if (!t)
            continue;
        comp_opid(O_R_ADDR, recvar(pre, t, VI_RECARR));
        path(t, text + i + 1, len - i - 1);
        comp.p = after;
        return true;
    }
    return false;
}

void ctrl_recload(void)
{
    const uint8_t et = ctrl.ref.et;
    if (et >= ET_REC)
        mismatch();
    if (et == ET_SD) {
        comp_opw(O_SF_LD, ctrl.ref.off);
        comp_word(ctrl.ref.fix);
        comp.type = T_STR;
    } else {
        comp_opw(O_FL_LD, ctrl.ref.off);
        comp_byte(et);
        if (et == ET_SH) {
            expr.sh = SH_SHORT;
            expr.shend = comp.out;
        }
        comp.type = expr_ktype[comp_etkind[et]];
    }
    comp.iload = comp.iloadend = nullptr;
}

void ctrl_recstore(void)
{
    const uint8_t et = ctrl.fieldet;
    if (et == ET_SD) {
        comp_tostring();
        comp_opw(O_SF_ST, comp.tgt);
        comp_word(ctrl.fieldfix);
        return;
    }
    comp_toinfo(comp_etkind[et]);
    comp_opw(O_FL_ST, comp.tgt);
    comp_byte(et);
}

/* ct_rcopy: a = b of two records of one TYPE: their addresses, R.COPY. */
static bool reccopy(void)
{
    const uint8_t *const p = comp.p;
    uint8_t *const out = comp.out;
    if (ctrl_recref() && ctrl.ref.et >= ET_REC) {
        const uint8_t et = ctrl.ref.et;
        if (comp_cur() == TK_EQ) {
            comp.p++;
            if (comp_cur() == M_VAR && ctrl_recref() && ctrl.ref.et == et && comp_end()) {
                comp_opw(O_R_COPY, ctrl.ref.fix);
                return true;
            }
        }
    }
    comp.p = p;
    comp.out = out;
    return false;
}

/* ---- procedures */

/* ct_proc: the procedure the name at comp.p is, its index to *idx, comp.p past the name; null
   none, comp.p kept. */
static const Proc *proc(uint8_t *const idx)
{
    const uint8_t *const p = comp.p;
    if (*p != M_VAR)
        return nullptr;
    const Name *const n = symb_name(comp_nameof(p));
    if (!(n->flags & (NM_SUB | NM_FUNC | NM_FN)) || !n->proc)
        return nullptr;
    comp.p = comp_skipname(p);
    *idx = n->proc;
    return &blok_procs[n->proc];
}

/* ct_pdecl: the name at comp.p a SUB's or FUNCTION's that a DECLARE or a definition named. */
static bool declname(void)
{
    return *comp.p == M_VAR &&
           symb_name(comp_nameof(comp.p))->flags & (NM_SUB | NM_FUNC | NM_FN);
}

/* ct_pchk: in the IDE's check of a line alone, a procedure its declaration named passes:
   blok_procs has it only once linked. */
static bool checkonly(void) { return comp.check && declname(); }

void ctrl_undef(const uint8_t err)
{
    comp_int(err);
    comp_call(K_ERROR);
}

void ctrl_dropargs(const bool paren)
{
    uint8_t *const out = comp.out;
    for (;;) {
        if (paren ? *comp.p == ')' : comp_end())
            break;
        if (*comp.p == TK_BYVAL)
            comp.p++;
        const uint8_t *const p = comp.p;
        const uint8_t *const after = *p == M_VAR ? comp_skipname(p) : p;
        if (*p == M_VAR && after[0] == '(' && after[1] == ')')
            comp.p = after + 2; /* a() an array whole */
        else
            expr_compile();
        if (*comp.p != ',')
            break;
        comp.p++;
    }
    comp.out = out;
}

/* ct_aend: c ends an argument. */
static bool aend(const uint8_t c) { return !c || c == ',' || c == ')' || c == ':' || c == TK_REM; }

/* ct_ptm: variable id the parameter's type; else Parameter type mismatch. */
static void ptm(const uint16_t id)
{
    if (symb_var(id)->type != symb_var(param)->type)
        repl_error(E_PTM);
}

/* An array parameter's argument: n() by its id, twice (its ref: PRET's store back); a record
   parameter's n by its id, FFFFh. */
static void arrayarg(void)
{
    if (comp_cur() != M_VAR)
        synerr();
    const uint16_t name = comp_nameof(comp.p);
    const uint8_t *const after = comp_skipname(comp.p);
    if (*after == '(') {
        const uint8_t t = scop_rec(name, true);
        uint16_t id;
        if (t) {
            comp.p = after;
            id = ctrl_recarray(name, t);
        } else
            id = comp_var(0, 0);
        if (!(symb_var(id)->info & VI_ARR))
            synerr();
        ptm(id);
        comp_open();
        comp_close();
        comp_int(id);
        comp_int(id);
        return;
    }
    const uint8_t t = scop_rec(name, false);
    if (!t)
        synerr();
    comp.p = after;
    comp_int(recvar(name, t, VI_RECARR));
    comp_int(0xFFFF);
}

/* ct_arg: an argument to parameter par: a variable alone by reference (V.ADDR), an element or a
   record's number field by P.REF, else the value and FFFFh. A CONST, BYVAL's, a DEF FN's: the
   value. */
static void arg(const uint16_t par)
{
    param = par;
    const uint8_t info = symb_var(par)->info;
    if (info & VI_ARR) {
        arrayarg();
        return;
    }
    if (!byval && comp_cur() == M_VAR) {
        const uint16_t name = comp_nameof(comp.p);
        if (!(symb_name(name)->flags & (NM_SUB | NM_FUNC | NM_FN)) && !scop_con(name)) {
            const uint8_t *const p = comp.p;
            const uint8_t *const after = comp_skipname(p);
            uint8_t *const out = comp.out;
            const uint8_t ptype = symb_var(par)->type;
            if (ctrl_recref()) { /* a field */
                const uint8_t et = ctrl.ref.et;
                if (!(et == ET_SD && ctrl.ref.fix) && et != ET_SH && et < ET_REC && et == ptype &&
                    aend(comp_cur())) {
                    comp_opw(O_P_REF, ctrl.ref.off);
                    return;
                }
            } else if (*after == '(') { /* an element */
                const uint16_t id = comp_var(0, 0);
                const Var *const v = symb_var(id);
                if (v->info & VI_ARR && !scop_fix(v->name, true) && v->type == ptype) {
                    const uint8_t n = expr_subs();
                    comp_opw(O_RA_ADDR, id);
                    comp_byte(n);
                    if (aend(comp_cur())) {
                        comp_opw(O_P_REF, 0);
                        return;
                    }
                }
            }
            comp.p = p;
            comp.out = out;
            if (aend(*after)) { /* a variable alone: its cell */
                const uint16_t id = comp_var(0, 0);
                ptm(id);
                comp_int(0);
                comp_opid(O_V_ADDR, id);
                return;
            }
        }
    }
    expr_compile();
    comp_toinfo(info);
    comp_int(0xFFFF);
}

/* ct_args: for each of the procedure's parameters, its argument. */
static void args(const Proc *const pr)
{
    const bool def = pr->flags & PF_DEF; /* a DEF FN's: by value */
    for (uint32_t i = 0; i < pr->npar; i++) {
        if (i) {
            if (comp_cur() != ',')
                synerr();
            comp.p++;
        }
        byval = blok_byval[pr->par + i] || def;
        arg(blok_params[pr->par + i]);
    }
}

/* ( args ), if there. */
static void argsparen(const Proc *const pr)
{
    if (*comp.p != '(')
        return;
    comp.p++;
    args(pr);
    comp_close();
}

static void pcall(const uint8_t idx, const Proc *const pr)
{
    comp_opb(O_PCALL, idx);
    emitjump(pr->entry);
}

void ctrl_fcall(void)
{
    uint8_t idx;
    const Proc *const pr = proc(&idx);
    if (!pr)
        synerr();
    argsparen(pr);
    pcall(idx, pr);
    comp.type = expr_ktype[symb_var(pr->result)->info & (SK_DBL | VK_MASK)];
    comp.iload = comp.iloadend = nullptr;
}

/* CALL n [( args )]. */
static void call(void)
{
    comp.p++;
    uint8_t idx;
    const Proc *const pr = proc(&idx);
    if (!pr) {
        if (!declname())
            synerr();
        if (comp.check) {
            skip();
            return;
        }
        skipvar(); /* DECLAREd only: its arguments, then Subprogram not defined when run */
        if (*comp.p == '(') {
            comp.p++;
            ctrl_dropargs(true);
            comp_close();
        }
        ctrl_undef(ERR_NO_SUB);
        return;
    }
    argsparen(pr);
    pcall(idx, pr);
}

/* ct_scall: n args, a SUB's call without CALL; another name is not one. */
static bool subcall(void)
{
    const uint8_t *const start = comp.p;
    uint8_t idx;
    const Proc *const pr = proc(&idx);
    if (pr) {
        if (pr->kind == PK_SUB) {
            if (!comp_end()) {
                if (comp_cur() == TK_EQ) /* a SUB's name assigned */
                    synerr();
                args(pr);
            }
            pcall(idx, pr);
            return true;
        }
        if (idx != comp.proc && comp_cur() == TK_EQ) /* f = e: in FUNCTION f only */
            repl_error(E_VR);
    }
    comp.p = start;
    if (checkonly()) {
        skip();
        return true;
    }
    if (*comp.p != M_VAR)
        return false;
    const Name *const n = symb_name(comp_nameof(comp.p));
    if (!(n->flags & NM_SUB) || n->proc)
        return false;
    skipvar(); /* a SUB DECLAREd only */
    if (!comp_end()) {
        if (comp_cur() == TK_EQ)
            synerr();
        ctrl_dropargs(false);
    }
    ctrl_undef(ERR_NO_SUB);
    return true;
}

/* SUB, FUNCTION: run into, passed: JMP past its END. */
static void sub(void)
{
    comp.p++;
    uint8_t idx;
    const Proc *const pr = proc(&idx);
    if (pr)
        jmp(pr->skip);
    else if (!checkonly())
        synerr();
    skip();
}

/* END or EXIT SUB, FUNCTION, DEF: PRET. */
static void pret(void)
{
    comp.p += 2;
    comp_op(O_PRET);
}

/* DEF FNn [(params)]: passed as a FUNCTION; with = e, JMP past it, then the result e, PRET. */
static void deffn(void)
{
    const uint8_t *const name = ++comp.p;
    uint8_t idx;
    const Proc *const pr = proc(&idx);
    if (!pr) {
        if (!checkonly())
            synerr();
        skip();
        return;
    }
    jmp(pr->skip);
    skipparen();
    if (comp_cur() != TK_EQ) {
        skip();
        return;
    }
    const uint8_t *const eq = comp.p;
    comp.p = name;
    comp_target(shaped(1)); /* its result */
    comp.p = eq + 1;
    expr_compile();
    comp_store();
    comp_op(O_PRET);
}

/* DEF SEG [= s]: no code, flat; DEF FN. */
static bool defseg(void)
{
    if (comp.p[1] == TK_X && comp.p[2] == TK1_SEG) {
        skip();
        return true;
    }
    if (comp.p[1] != M_VAR)
        return false;
    deffn();
    return true;
}

/* ---- other statements */

/* REDIM [PRESERVE]: DIM's grammar, each CALL DIM marked REDIM's. */
static void redim(void)
{
    comp.p++;
    comp.redim = DIM_REDIM;
    if (comp.p[0] == TK_X && comp.p[1] == TK1_PRESERVE) {
        comp.p += 2;
        comp.redim = DIM_REDIM | DIM_PRESERVE;
    }
    comp_dfa(lexr_ps_entry[TK_DIM - 0x80]);
    comp.redim = 0;
}

/* ERASE a {, a}: each array's id, CALL ERASE. */
static void erase(void)
{
    comp.p++;
    do {
        uint16_t id = shaped(2);
        const uint16_t name = symb_var(id)->name;
        const uint8_t t = scop_rec(name, true); /* a record array: its own variable */
        if (t)
            id = ctrl_recarray(name, t);
        comp_int(id);
        comp_call(K_ERASE);
    } while (comp_optcomma());
}

/* ct_evk: the TK_X word at comp.p an event's: its EK_*, comp.p past it. */
static bool evkind(uint8_t *const kind)
{
    switch (comp.p[1]) {
    case TK1_TIMER: *kind = EK_TIMER; break;
    case TK1_KEY: *kind = EK_KEY; break;
    case TK1_COM: *kind = EK_COM; break;
    case TK1_PEN: *kind = EK_PEN; break;
    case TK1_STRIG: *kind = EK_STRIG; break;
    default: return false;
    }
    comp.p += 2;
    return true;
}

/* ct_evn: the event's (n): KEY's, COM's, STRIG's; TIMER's in ON TIMER; else 0. */
static void evnum(const uint8_t kind)
{
    if (kind != EK_PEN) {
        if (comp_cur() == '(') {
            expr_paren();
            comp_toint();
            return;
        }
        if (kind != EK_TIMER)
            synerr();
    }
    comp_int(0);
}

/* ON [LOCAL] ERROR GOTO line: ONERR or ONLERR num addr; ON event(n) GOSUB line: ONEV kind num
   addr; another ON is not one. */
static bool onerror(void)
{
    const uint8_t *const start = comp.p++;
    if (comp_cur() != TK_X) {
        comp.p = start;
        return false;
    }
    uint8_t kind;
    if (evkind(&kind)) {
        evnum(kind);
        if (comp_cur() != TK_GOSUB)
            synerr();
        comp.p++;
        comp.modlabel = true; /* an event's handler: the module's */
        comp_lnum();
        comp_opb(O_ONEV, kind);
        comp_lref();
        return true;
    }
    uint8_t op = O_ONERR;
    if (comp.p[1] == TK1_LOCAL) { /* PDS's ON LOCAL ERROR */
        comp.p += 2;
        if (comp_cur() != TK_X)
            synerr();
        op = O_ONLERR;
    }
    if (comp.p[1] != TK1_ERROR) {
        comp.p = start;
        return false;
    }
    comp.p += 2;
    if (comp_cur() != TK_GOTO) {
        comp.dg = DG_GOTO_MISSING;
        synerr();
    }
    comp.p++;
    comp.modlabel = op == O_ONERR; /* ON ERROR's handler: the module's */
    comp_lnum();
    comp_op(op);
    comp_lref();
    return true;
}

/* ct_evs: event(n) ON | OFF | STOP: CALL EVST ( n kind*4+state -- ); KEY n, s: CALL KEYDEF; KEY
   ON, OFF, LIST: the screen's, nothing. */
static bool evstate(void)
{
    uint8_t kind;
    if (!evkind(&kind))
        return false;
    if (kind == EK_KEY) {
        const uint8_t c = comp_cur();
        if (c == TK_ON || c == TK_LIST) {
            comp.p++;
            return true;
        }
        if (c == TK_X && comp.p[1] == TK1_OFF) {
            comp.p += 2;
            return true;
        }
        if (c != '(') {
            expr_compile();
            comp_toint();
            comp_comma();
            expr_compile();
            comp_tostring();
            comp_call(K_KEYDEF);
            return true;
        }
    }
    evnum(kind);
    const uint8_t c = comp_cur();
    uint8_t state;
    if (c == TK_X) {
        if (comp.p[1] != TK1_OFF)
            synerr();
        comp.p++;
        state = ES_OFF;
    } else if (c == TK_ON)
        state = ES_ON;
    else if (c == TK_STOP)
        state = ES_STOP;
    else
        synerr();
    comp.p++;
    comp_int(kind * 4u + state);
    comp_call(K_EVST);
    return true;
}

/* COMMON [SHARED] [/block/] v[()] [AS type] {, ..}: each variable's id, CALL CMN. */
static void common(void)
{
    comp.p += 2;
    if (comp_cur() == TK_SHARED)
        comp.p++;
    if (comp_cur() == TK_DIV) {
        comp.p++;
        if (comp_cur() != M_VAR)
            synerr();
        skipvar();
        if (comp_cur() != TK_DIV)
            synerr();
        comp.p++;
    }
    do {
        const uint16_t id = comp_name();
        if (comp_cur() == '(') {
            comp.p++;
            if (comp_cur() != ')')
                synerr();
            comp.p++;
        }
        if (comp_cur() == TK_AS) { /* AS type [* n] */
            comp.p++;
            const uint8_t c = comp_cur();
            if (c == M_VAR)
                skipvar();
            else if (c == TK_X)
                comp.p += 2;
            else {
                comp.p++;
                if (comp_cur() == TK_MUL) {
                    comp.p++;
                    if (comp_cur() != M_INT)
                        synerr();
                    comp.p += INT_TOKEN_SIZE;
                }
            }
        }
        comp_int(id);
        comp_call(K_CMN);
    } while (comp_optcomma());
}

/* RESUME [NEXT | 0 | line]: RESUME mode num addr. */
static void resume(void)
{
    comp.p += 2;
    comp.lnum = 0;
    comp.jline = 0;
    ResumeMode mode = RESUME_AGAIN;
    if (comp_cur() == TK_NEXT) {
        comp.p++;
        mode = RESUME_NEXT;
    } else if (!comp_end()) {
        comp_lnum();
        if (comp.lnum || comp.jline) /* not RESUME 0 */
            mode = RESUME_LINE;
    }
    comp_opb(O_RESUME, mode);
    comp_lref();
}

static bool xstmt(void)
{
    switch (comp.p[1]) {
    case TK1_COMMON: common(); return true;
    case TK1_DEFSHORT: skip(); return true;
    case TK1_RESUME: resume(); return true;
    }
    return evstate();
}

/* MID$(v, i [, n]) = s: v = its value with s over it. */
static void mids(void)
{
    comp.p++;
    comp_open();
    const uint8_t *const v = comp.p;
    expr_target();
    if ((comp.tgtinfo & VK_MASK) != VK_STR)
        mismatch();
    const uint16_t tgt = comp.tgt;
    comp.p = v;
    expr_load();
    comp_comma();
    expr_compile();
    comp_toint();
    if (comp_optcomma()) {
        expr_compile();
        comp_toint();
    } else
        comp_int(SMAX);
    comp_close();
    comp_equals();
    expr_compile();
    comp_tostring();
    comp_call(K_MIDS);
    comp.tgt = tgt;
    comp.type = T_STR;
    comp_store();
}

/* A statement starting with a name: a TYPE's field, a record's copy, a SUB's call. */
static bool namestmt(void)
{
    if (*comp_skipname(comp.p) == TK_AS) {
        skip();
        return true;
    }
    return reccopy() || subcall();
}

/* LET: a record's copy, else not one. */
static bool let(void)
{
    comp.p++;
    if (*comp.p == M_VAR && reccopy())
        return true;
    comp.p--;
    return false;
}

bool ctrl_stmt(void)
{
    const uint8_t c = comp.p[0];
    const uint8_t n = comp.p[1];
    switch (c) {
    case TK_ELSEIF: elseif(); return true;
    case TK_ELSE: blockelse(); return true;
    case TK_ENDIF: comp.p++; return true;
    case TK_SELECT: select(); return true;
    case TK_CASE: casestmt(); return true;
    case TK_DO: doloop(); return true;
    case TK_LOOP: loop(); return true;
    case TK_WHILE: whileloop(); return true;
    case TK_WEND: wend(); return true;
    case M_VAR: return namestmt();
    case TK_LET: return let();
    case TK_CALL: call(); return true;
    case TK_SUB:
    case TK_FUNCTION: sub(); return true;
    case TK_DEF: return defseg();
    case TK_MID: mids(); return true;
    case TK_ON: return onerror();
    case TK_REDIM: redim(); return true;
    case TK_ERASE: erase(); return true;
    case TK_X: return xstmt();
    case TK_TYPE:
    case TK_DECLARE:
    case TK_OPTION:
    case TK_SHARED:
    case TK_STATIC: skip(); return true; /* entered with the line (decl_line), no code */
    case TK_EXIT:
        if (n == TK_SUB || n == TK_FUNCTION || n == TK_DEF)
            pret();
        else
            exitblock();
        return true;
    case TK_END:
        if (n == TK_IF || n == TK_SELECT || n == TK_TYPE) { /* no code */
            comp.p += 2;
            return true;
        }
        if (n == TK_SUB || n == TK_FUNCTION || n == TK_DEF) {
            pret();
            return true;
        }
    }
    return false;
}
