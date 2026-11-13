/* The compiler (comp/comp.asm): a token body to stack-machine code. */
#include "comp/comp.h"
#include "comp/blok.h"
#include "comp/ctrl.h"
#include "comp/decl.h"
#include "comp/dfa.h"
#include "comp/expr.h"
#include "io/cons.h"
#include "io/msg.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "mem/scop.h"
#include "mem/symb.h"
#include "num/fmth.h"
#include "vm/dbug.h"

Comp comp;
uint8_t *comp_cbuf;
uint8_t comp_xbuf[XLIM + 12];
const uint8_t comp_etkind[7] = {0, 1, 1, 0, SK_DBL, 2, 1};

static uint64_t dmask; /* dimsubs: bit i, subscript i its hi only */

void comp_init(void)
{
    comp_cbuf = at(allo_carve(1 + LMAX + 64)) + 1; /* the byte before: lexr's line before the body */
    comp.buf = comp_xbuf;
    comp.dfa = 0xFFFF;
}

[[noreturn]] static void synerr(void) { repl_error(E_SN); }
[[noreturn]] static void mismatch(void) { repl_error(E_TM); }

/* ---- emitting */

void comp_byte(uint8_t b)
{
    if (comp.out >= comp.lim)
        comp_fail(E_OM); /* cm_om: the code is full */
    *comp.out++ = b;
}

void comp_opb(uint8_t op, uint8_t b)
{
    comp_op(op);
    comp_byte(b);
}

void comp_opw(uint8_t op, uint16_t w)
{
    comp.lnum = w; /* the asm's wv: the word */
    comp_op(op);
    comp_byte((uint8_t)w);
    comp_byte((uint8_t)(w >> 8));
}

void comp_dword(uint32_t d)
{
    for (int i = 0; i < 4; i++, d >>= 8)
        comp_byte((uint8_t)d);
}

void comp_lref(void)
{
    comp_byte((uint8_t)comp.lnum);
    comp_byte((uint8_t)(comp.lnum >> 8));
    comp_dword(comp.jline);
}

void comp_insert(uint8_t b, uint8_t *where)
{
    if (comp.out >= comp.lim)
        comp_fail(E_OM);
    memmove(where + 1, where, (size_t)(comp.out - where));
    comp.out++;
    *where = b;
    if ((uintptr_t)comp.iload >= (uintptr_t)where) {
        comp.iload++;
        comp.iloadend++;
    }
    if ((uintptr_t)comp.cmp >= (uintptr_t)where) {
        comp.cmp++;
        comp.cmpend++;
    }
}

void comp_int(uint32_t n)
{
    comp.type = T_INT;
    comp.iload = comp.out;
    if (n > 0xFFFF) {
        comp_op(O_I_LD_I32);
        comp_dword(n);
    } else if (n > 0xFF)
        comp_opw(O_I_LD_I16, (uint16_t)n);
    else
        comp_opb(O_I_LD_U8, (uint8_t)n);
    comp.iloadend = comp.out;
    expr.kend = comp.out;
}

void comp_call(uint8_t k) { comp_opb(O_CALL, k); }

/* cm_vop: X.LD.V or X.ST.V of a scalar parameter of the procedure compiled: V.LD, V.ST id kind
   instead (kind 0 4 bytes, 1 8, 2 a Desc). */
static bool parop(uint8_t op, uint16_t id)
{
    uint8_t vop, kind;
    switch (op) {
    case O_I_LD_V: case O_F_LD_V: vop = O_V_LD; kind = 0; break;
    case O_D_LD_V: vop = O_V_LD; kind = 1; break;
    case O_S_LD_V: vop = O_V_LD; kind = 2; break;
    case O_I_ST_V: case O_F_ST_V: vop = O_V_ST; kind = 0; break;
    case O_D_ST_V: vop = O_V_ST; kind = 1; break;
    case O_S_ST_V: vop = O_V_ST; kind = 2; break;
    default: return false;
    }
    if (!comp_ispar(id))
        return false;
    comp_opw(vop, id);
    comp_byte(kind);
    return true;
}

void comp_opid(uint8_t op, uint16_t id)
{
    if (!parop(op, id))
        comp_opw(op, id);
}

uint8_t *comp_intleaf(void)
{
    if (comp.out != comp.iloadend)
        return nullptr;
    uint8_t op = *comp.iload;
    return op == O_I_LD_V || op == O_I_LD_U8 || op == O_I_LD_I32 || op == O_I_LD_I16 ? comp.iload
                                                                                     : nullptr;
}

/* ---- types */

void comp_toint(void)
{
    if (comp.type & T_STR)
        mismatch();
    if (comp.type == T_INT)
        return;
    uint8_t op = comp.type == T_DBL ? O_D_TOI : O_F_TOI;
    comp.type = T_INT;
    comp_op(op);
}

void comp_todouble(void)
{
    if (comp.type & T_STR)
        mismatch();
    uint8_t op;
    if (comp.type == T_INT)
        op = O_I_TOD;
    else if (comp.type == T_FLT)
        op = O_F_TOD;
    else
        return;
    comp.type = T_DBL;
    comp_op(op);
}

void comp_dtest(void)
{
    if (comp.type != T_DBL)
        return;
    comp.type = T_INT;
    comp_op(O_D_NZ);
}

/* A byte or an address: a U8 load needs nothing. */
static void tonarrow(uint8_t fop, uint8_t iop)
{
    if (comp.type & T_STR)
        mismatch();
    if (comp.type == T_DBL)
        comp_toint();
    if (comp.type == T_INT) {
        uint8_t *leaf = comp_intleaf();
        if (leaf && *leaf == O_I_LD_U8)
            return;
        fop = iop;
    }
    comp.type = T_INT;
    comp_op(fop);
}

void comp_tobyte(void) { tonarrow(O_F_TOB, O_I_TOB); }
void comp_toaddr(void) { tonarrow(O_F_TOA, O_I_TOA); }

void comp_tostring(void)
{
    if (!(comp.type & T_STR))
        mismatch();
}

void comp_tofloat(void)
{
    if (comp.type & T_STR)
        mismatch();
    if (comp.type == T_FLT)
        return;
    if (comp.type == T_DBL) {
        comp.type = T_FLT;
        comp_op(O_D_TOF);
        return;
    }
    comp.type = T_FLT;
    uint8_t *leaf = comp_intleaf();
    if (!leaf) {
        comp_op(O_I_TOF);
        return;
    }
    switch (*leaf) {
    case O_I_LD_V:
        *leaf = O_F_LD_VI;
        return;
    case O_I_LD_U8:
        *leaf = O_F_LD_U8;
        return;
    }
    Sngl f = *leaf == O_I_LD_I32 ? fmth_from_i32((int32_t)load32(leaf + 1))
                                 : fmth_from_u16(load16(leaf + 1));
    comp.out = leaf; /* F.LD.F32 in its place */
    comp_op(O_F_LD_F32);
    comp_dword(f);
}

void comp_target(uint16_t id)
{
    comp.tgt = id;
    comp.tgtinfo = symb_var(id)->info;
    comp.tgtsubs = 0;
}

void comp_store(void)
{
    if (comp.tgtsubs == TGT_FIELD) {
        ctrl_recstore();
        return;
    }
    uint8_t info = comp.tgtinfo, subs = comp.tgtsubs, k;
    if (info & SK_DBL) { /* a DOUBLE: D.ST.V, .A1, .A */
        comp_todouble();
        uint8_t op = !subs ? O_D_ST_V : subs == 1 ? O_D_ST_A1 : O_D_ST_A;
        comp_opid(op, comp.tgt);
        if (op == O_D_ST_A)
            comp_byte(subs);
        return;
    }
    switch (info & 3) {
    case 1: /* an INT; a SHORT: Overflow past 16 bits, unless it is one */
        expr_shclass();
        comp_toint();
        if (symb_var(comp.tgt)->type == ET_SH && !expr.shclass)
            comp_op(O_I_CHK16);
        k = 0;
        break;
    case 2: { /* a STRING * n: n chars */
        comp_tostring();
        uint16_t fix = scop_fix(symb_var(comp.tgt)->name, subs != 0);
        if (fix) {
            comp_int(fix);
            comp_call(K_SFIX);
        }
        k = 2;
        break;
    }
    case 3:
        synerr();
    default:
        comp_tofloat();
        k = 1;
    }
    if (!subs)
        comp_opid(O_I_ST_V + k, comp.tgt);
    else if (subs == 1)
        comp_opid(O_I_ST_A1 + k, comp.tgt);
    else {
        comp_opid(O_I_ST_A + k, comp.tgt);
        comp_byte(subs);
    }
}

/* ---- the token reader */

uint8_t comp_peek(const uint8_t **p)
{
    while (**p == ' ')
        (*p)++;
    return **p;
}

uint8_t comp_next(const uint8_t **p)
{
    (*p)++;
    return comp_peek(p);
}

bool comp_eos(const uint8_t **p)
{
    uint8_t c = comp_peek(p);
    if (!c || c == ':' || c == TK_REM)
        return true;
    return c == TK_ELSE && comp.oneif;
}

/* st_eosn: comp_eos a byte on. */
static bool eosnext(void)
{
    comp.p++;
    return comp_end();
}

void comp_need(uint8_t b)
{
    if (comp_cur() != b)
        synerr();
    comp_curn();
}

void comp_equals(void) { comp_need(TK_EQ); }

bool comp_optcomma(void)
{
    if (comp_cur() != ',')
        return false;
    comp.p++;
    return true;
}

/* Label key of DX's kind: jline, in the scope's own or, when modlabel (cleared), the module's. */
static void lfind(uint16_t key, uint16_t kind)
{
    uint16_t scope = comp.modlabel ? 0 : scop_own();
    comp.modlabel = false;
    comp.jline = scop_lfind(scope | kind, key);
}

uint8_t comp_lnum(void)
{
    comp.jline = 0;
    uint8_t c = comp_cur();
    if (c == M_VAR) {
        lfind(comp_nameof(comp.p), 0);
        comp.lnum = NONUM;
        comp.p += comp.p[1] + 4;
        return comp_cur();
    }
    if (c != M_INT) {
        comp.modlabel = false;
        comp.lnum = 0;
        return c;
    }
    uint32_t n = load32(comp.p + 1);
    if (n >= 64000)
        synerr();
    comp.lnum = (uint16_t)n;
    comp.p += 5;
    if (n) /* 0: ON ERROR GOTO 0's, none */
        lfind((uint16_t)n, SC_NUM);
    comp.modlabel = false;
    return comp_cur();
}

/* cm_lnld: I.LD.I16 of the line number; a label's line as LABEL_LINE + its line. */
static void lnumload(void)
{
    if (comp.lnum == NONUM)
        comp_int(comp.jline + LABEL_LINE);
    else
        comp_opw(O_I_LD_I16, comp.lnum);
}

/* ---- names and variables */

uint8_t comp_suffix(uint8_t c)
{
    switch (c) {
    case '!': return ET_R4;
    case '#': return ET_R8;
    case '%': return ET_I2;
    case '&': return ET_I4;
    case '$': return ET_SD;
    default: return 0;
    }
}

const uint8_t *comp_label(const uint8_t *body)
{
    if (*body != M_VAR)
        return nullptr;
    const uint8_t *colon = body + body[1] + 4;
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

bool comp_ispar(uint16_t id)
{
    if (symb_var(id)->info & VI_ARR || !comp.proc)
        return false;
    const Proc *p = &blok_procs[comp.proc];
    for (uint32_t i = 0; i < p->npar; i++)
        if (blok_params[p->par + i] == id)
            return true;
    return false;
}

bool comp_param(uint16_t name, uint8_t array, uint16_t *id)
{
    if (!comp.proc)
        return false;
    const Proc *p = &blok_procs[comp.proc];
    for (uint32_t i = 0; i < p->npar; i++) {
        uint16_t v = blok_params[p->par + i];
        const Var *var = symb_var(v);
        if (var->name == name && !((var->info ^ array) & 0x80)) {
            *id = v;
            return true;
        }
    }
    return false;
}

void comp_scope(uint16_t name)
{
    comp.isstatic = false;
    comp.scope = comp.proc;
    if (!comp.proc)
        return;
    uint16_t scope;
    const Binding *b = nullptr;
    if (!(symb_name(name)->flags & NM_LBL))
        b = scop_look(name, &scope);
    if (!b || !scope) {
        comp.scope = 0; /* the module's */
        return;
    }
    if (b->flags & BF_STAT || blok_procs[comp.proc].flags & PF_STAT)
        comp.isstatic = true; /* STATIC */
}

/* var_decl: under OPTION EXPLICIT, name declared, or a procedure's; else Variable not declared. */
static void declared(uint16_t name)
{
    if (!decl.explicit_ || symb_name(name)->flags & (NM_SUB | NM_FUNC | NM_FN))
        return;
    if (!scop_known(name))
        repl_error(E_VND);
}

uint16_t comp_var(uint8_t shape, uint8_t et)
{
    const uint8_t *q = comp.p;
    uint8_t len = q[1];
    uint8_t last = q[len + 1], first = q[2];
    uint16_t name = load16(q + len + 2);
    comp.p = q + len + 4;
    uint8_t kind = 0;
    if (shape > 1 || (!shape && *comp.p == '('))
        kind = VI_ARR;
    uint8_t type = et;
    if (!type) {
        uint8_t t = comp_suffix(last);
        if (t)
            type = scop_sfx(name, t, kind != 0);
        else {
            uint16_t id;
            if (comp_param(name, kind, &id)) /* no suffix: a parameter's own type */
                return id;
            type = scop_type(name, kind != 0);
            if (!type) {
                uint8_t f = first >= 'a' && first <= 'z' ? first - 32 : first;
                type = symb.deft[f - 'A'];
            }
        }
    }
    declared(name);
    if (type >= ET_REC) { /* a record: ctrl_recarray's, recvar's kind */
        if (!kind)
            kind = 0x81;
    } else
        kind |= comp_etkind[type];
    comp_scope(name);
    return symb_find_var(name, type, kind, comp.scope, comp.isstatic);
}

uint16_t comp_name(void)
{
    if (comp_cur() != M_VAR)
        synerr();
    return comp_var(0, 0);
}

/* ---- the statement map, DBG */

static void mapstmt(uint32_t bodyoff)
{
    uint8_t *e = comp.map - (comp.nstmt * 8 + sizeof(Stmt));
    if (e < comp.out + 8) { /* no room: the code is full */
        comp.lim = comp.out;
        return;
    }
    *(Stmt *)e = (Stmt){.body = bodyoff, .code = (uint32_t)(comp.out - comp.buf)}; /* packed: any address */
    comp.lim = e - 12;
    comp.nstmt++;
}

/* The statement at comp.p: its body offset, its code's place. */
static void mark(void) { mapstmt((uint32_t)(comp.p - comp.body)); }

/* DBG before a program statement's code, when the debugger wants it; not SUB's, FUNCTION's. */
static void dbg(void)
{
    comp.dbg = nullptr;
    if (dbug.emit && comp.line != DLINE && *comp.p != TK_SUB && *comp.p != TK_FUNCTION) {
        *comp.out++ = O_DBG;
        comp.dbg = comp.out;
    }
}

/* After the statement: its DBG gone when it made no code (DECLARE, DEFINT, ...). */
static void dbgdrop(void)
{
    if (comp.dbg == comp.out)
        comp.out--;
    comp.dbg = nullptr;
}

/* ---- the line */

/* cm_fin: the line's end, EOL. */
static void finish(void)
{
    mapstmt(BOFF);
    *comp.out++ = O_EOL;
}

/* REM: the compile is over. */
[[noreturn]] static void done(void)
{
    finish();
    longjmp(comp.unwind, 1);
}

void comp_fail(uint8_t err)
{
    uint8_t *o = comp.out;
    *o++ = O_ERR;
    *o++ = err;
    *o++ = comp.dg; /* its words, and where in the body */
    comp.dg = 0;
    uint32_t atoff = (uint32_t)(comp.p - comp.body);
    store32(o, atoff);
    memcpy(o + 4, &comp.dfa, 2);
    comp.out = o + 6;
    comp.dfa = 0xFFFF;
    done();
}

static uint32_t pslook(uint32_t st, uint8_t cls)
{
    for (uint32_t i = lexr_ps_first[st]; i < lexr_ps_first[st + 1]; i++)
        if (lexr_ps_xclass[i] == cls)
            return lexr_ps_xentry[i];
    return lexr_ps_default[st];
}

static void (*const actions[A_END - A_BASE])(void);

void comp_dfa(uint16_t state)
{
    uint32_t st = state;
    for (;;) {
        uint32_t e = pslook(st, lexr_ps_class[comp_cur()]);
        uint8_t code = (uint8_t)e;
        if (code < PS_ACC) {
            st = e >> 8;
            if (code < A_BASE)
                comp_call(code);
            else
                actions[code - A_BASE]();
            continue;
        }
        if (code == PS_ACC)
            return;
        if (code == PS_ERR) { /* for the error's words: what it wanted */
            comp.dfa = (uint16_t)st;
            synerr();
        }
        st = e >> 8;
        if (code == PS_TOK)
            comp.p++;
    }
}

/* State st's names to names[n..]; past FFh (follow) the keywords of the state it leads to. */
static uint8_t expnames(uint32_t st, bool follow, const char *names[], uint8_t n)
{
    for (uint8_t cls = 0; cls < PS_NC; cls++) {
        uint32_t e = pslook(st, cls);
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

uint8_t comp_expnames(uint16_t state, const char *names[])
{
    return expnames(state, true, names, 0);
}

void comp_expected(uint16_t state)
{
    const char *names[PS_NC * 2];
    uint8_t n = comp_expnames(state, names);
    for (uint8_t i = 0; i < n; i++) {
        cons_puts(names[i]);
        uint8_t left = n - i - 1;
        if (left)
            cons_puts(left == 1 ? " or " : ", ");
    }
}

/* cm_tok: the statement at comp.p. */
static void token(void)
{
    mark();
    dbg();
    if (ctrl_stmt())
        return;
    uint8_t c = *comp.p;
    if (c == M_VAR) {
        comp_dfa(ST_LET);
        return;
    }
    if (c < 0x80)
        synerr();
    comp.tok = c;
    comp.kw = comp.p;
    uint16_t st;
    if (c == TK_X) { /* FFh n: its own table, past both */
        st = lexr_ps_entry1[comp.p[1]];
        comp.p++;
    } else
        st = lexr_ps_entry[c - 0x80];
    if (!st)
        synerr();
    comp_curn();
    comp_dfa(st);
}

/* The statements after comp.p, to the line's end. */
static void stmts(void)
{
    for (;;) { /* cm_new: comp.p before a statement */
        comp.then = false;
        if (!eosnext()) {
            token();
            dbgdrop(); /* cm_nx */
            if (!comp_end())
                synerr();
        }
        for (;;) { /* cm_ee: 0 or ' the line ends, ':' another statement, ELSE a one-line IF's */
            uint8_t c = *comp.p;
            if (!c || c == TK_REM)
                return;
            if (c == ':')
                break;
            comp_op(O_NXLN); /* ELSE: the THEN part leaves the line */
            c = comp_curn();
            if (!c)
                synerr();
            if (c == M_INT) {
                mark();
                comp_dfa(ST_GOTO); /* ELSE line */
            } else
                token();
            dbgdrop();
            if (!comp_end())
                synerr();
        }
    }
}

void comp_text(const uint8_t *body)
{
    comp.busy = true;
    comp.depth = 0;
    comp.body = body;
    comp.nstmt = 0;
    comp.oneif = false;
    comp.dg = 0;
    comp.dfa = 0xFFFF;
    comp.iload = comp.iloadend = comp.cmpend = nullptr;
    comp.out = comp.buf;
    comp.map = comp.buf + XLIM + 12; /* the map down from the top, 12 bytes above the code for
                                        ERR, EOL */
    comp.lim = comp.map - 12;
    const uint8_t *colon = comp_label(body); /* a label: on from its ':' */
    comp.p = colon ? colon : body - 1;
    if (!setjmp(comp.unwind)) {
        stmts();
        finish();
    }
    comp.busy = false;
    comp.len = (uint8_t)(comp.out - comp.buf);
}

void comp_line(void)
{
    comp.line = DLINE;
    scop_proc(comp.dlproc);
    decl_deftype(0xFFFFFFFF); /* the program's end's DEFtypes */
    comp.buf = comp_xbuf;
    comp_text(comp_cbuf);
}

/* ---- the actions */

/* IF expr THEN|GOTO line or statements: a compare just made becomes its CMPIF, else X.IF. */
static void act_if(void)
{
    expr_compile();
    comp_dtest();
    uint8_t c = *comp.p;
    if (c != TK_THEN && c != TK_GOTO)
        synerr();
    if (c == TK_THEN && (!comp.p[1] || comp.p[1] == TK_REM)) { /* a block IF */
        ctrl_blockif();
        return;
    }
    comp.oneif = true;
    uint32_t t;
    if (ctrl_else(&t))
        ctrl_jf(t); /* false: past the ELSE */
    else if (comp.out == comp.cmpend)
        *comp.cmp += O_I_CMPIF - O_I_CMP;
    else
        comp_op(comp.type == T_INT ? O_I_IF : comp.type == T_FLT ? O_F_IF : O_S_IF);
    comp.then = true;
    uint8_t kw = *comp.p; /* THEN or GOTO */
    c = comp_curn();
    if (kw == TK_GOTO || c == M_INT)
        comp_dfa(ST_GOTO);
    else
        token();
}

/* FOR's variable: the target of the start's store. */
static void act_fv(void)
{
    uint16_t id = comp_name();
    uint8_t info = symb_var(id)->info;
    if (info & VI_ARR)
        synerr();
    comp.forvar = id;
    comp_target(id);
    if (info & SK_DBL) /* a DOUBLE's FOR: not yet */
        mismatch();
    info &= 0x43;
    if (info == 2)
        mismatch();
    comp.forkind = info;
}

/* The value as FOR's variable wants it. */
static void act_fork(void)
{
    if (comp.forkind)
        comp_toint();
    else
        comp_tofloat();
}

static void act_for(uint8_t op)
{
    if (!comp.forkind)
        op++; /* F.FOR, F.FOR1 */
    comp_opid(op, comp.forvar);
    ctrl_forskip();
}
static void act_for2(void) { act_for(O_I_FOR); }
static void act_for1(void) { act_for(O_I_FOR1); }

/* NEXT.V id, or NEXT. */
static void act_nv(void)
{
    if (comp_cur() == M_VAR)
        comp_opid(O_NEXT_V, comp_var(0, 0));
    else
        comp_op(O_NEXT);
}

/* cm_skip: DATA's text, to a ':' outside quotes. */
static void skipdata(void)
{
    uint8_t q = 0;
    for (;; comp.p++) {
        uint8_t c = *comp.p;
        if (!c)
            return;
        if (c == '"')
            q++;
        else if (!(q & 1)) {
            if (c == ':')
                return;
            if (c == M_INT)
                comp.p += 4;
            else if (c == M_VAR)
                comp.p += comp.p[1] + 3;
        }
    }
}

/* DATA: no code, READ finds the text in the body. */
static void act_data(void)
{
    comp.p = comp.kw + 1;
    skipdata();
}

/* REM: the line's end. */
static void act_rem(void) { done(); }

/* RUN, RESTORE [line]: CALL, or the line and CALL .L. */
static void runres(uint8_t k)
{
    if (comp_end()) {
        comp_call(k);
        return;
    }
    comp.modlabel = true; /* the module's label */
    comp_lnum();
    lnumload();
    comp_call(k + 1);
}
static void act_run(void) { runres(K_RUN); }
static void act_res(void) { runres(K_RES); }

static void act_n0(void) { comp.nargs = 0; }
static void act_n1(void) { comp.nargs++; }
static void act_narg(void) { comp_int(comp.nargs); }

/* DIM's ( [lo TO] hi {, ..} ): lo hi, or hi and OPTION BASE's, each an INT; how many. */
static uint32_t dimsubs(void)
{
    comp_open();
    uint32_t n = 0;
    dmask = 0;
    do {
        expr_compile();
        comp_toint();
        if (comp_cur() == TK_TO) {
            comp.p++;
            expr_compile();
            comp_toint();
        } else {
            comp_int(decl.base);
            dmask |= (uint64_t)1 << (n & 63);
        }
        n++;
    } while (comp_optcomma());
    comp_close();
    return n;
}

/* DIM's array id: ( subscripts ), the mask as 2 INTs, its id, the count, CALL DIM. A STATIC's
   array is dimensioned once. */
static void dim1(uint16_t id)
{
    uint32_t flags = (comp.isstatic ? DIM_STATIC : 0) | comp.redim;
    uint32_t n = dimsubs();
    comp_int((uint32_t)dmask);
    comp_int((uint32_t)(dmask >> 32));
    comp_int(id);
    comp_int(n | flags);
    comp_call(K_DIM);
}

/* Past DIM's AS type [* n]: decl_line's. */
static void dimas(void)
{
    const uint8_t *p = comp.p;
    if (*p != TK_AS)
        return;
    if (p[1] == M_VAR) { /* AS a TYPE */
        comp.p = decl_item(p + 1);
        return;
    }
    if (p[1] == TK_X) { /* AS SHORT */
        comp.p = p + 3;
        return;
    }
    comp.p = p += 2;
    if (p[-1] == TK_STRING && *p == TK_MUL) /* STRING * n */
        comp.p += 6;
}

/* A DIM item: the subscripts, its id, n, CALL DIM; a scalar STRING * n: n spaces. */
static void act_dv(void)
{
    uint16_t id = comp_name();
    if (*comp.p == '(') {
        uint16_t name = symb_var(id)->name;
        uint8_t t = scop_rec(name, true); /* a record array: its own variable */
        if (t)
            id = ctrl_recarray(name, t);
        dim1(id);
        dimas();
        return;
    }
    dimas();
    uint16_t fix = scop_fix(symb_var(id)->name, false);
    if (!fix || symb_var(id)->info & VI_ARR)
        return;
    comp_int(fix);
    comp_call(K_SPACE);
    comp_opid(O_S_ST_V, id);
}

/* INPUT's, READ's, GET's value for the target, stored. */
static void readto(uint8_t k)
{
    comp.type = T_FLT;
    uint8_t info = comp.tgtinfo;
    if (info & SK_DBL && k != K_FGET) {
        k = k == K_FIN ? K_DIN : K_DREAD;
        comp.type = T_DBL;
    } else if ((info & 3) == 2) {
        k++;
        comp.type = T_STR;
    }
    comp_call(k);
    comp_store();
}
static void act_in(void) { readto(K_FIN); }
static void act_rd(void) { readto(K_FREAD); }
static void act_get(void) { readto(K_FGET); }

/* PRINT USING's items: each by its type; US.END, or US.ENDN when ; or , ends the list. */
static void using(void)
{
    for (;;) {
        if (comp_end())
            break;
        expr_compile();
        uint8_t t = comp.type;
        comp_call(t == T_STR ? K_USS : t == T_FLT ? K_USF : t == T_DBL ? K_USD : K_USI);
        if (comp_end())
            break;
        uint8_t c = *comp.p;
        if (c != ',' && c != ';')
            synerr();
        if (eosnext()) {
            comp_call(K_USEN);
            return;
        }
    }
    comp_call(K_USE);
}

/* PRINT items; PR.NL unless the list ends with ; , TAB( or SPC(. USING: using's items. */
static void act_print(void)
{
    if (comp_cur() == TK_X && comp.p[1] == TK1_USING) {
        comp.p += 2; /* USING format; items */
        expr_compile();
        comp_tostring();
        comp_call(K_USB);
        if (comp_cur() != ';')
            synerr();
        comp.p++;
        using();
        return;
    }
    if (comp_end()) {
        comp_call(K_PNL);
        return;
    }
    for (;;) {
        uint8_t c = *comp.p;
        if (c == ',')
            comp_call(K_PZONE);
        else if (c == TK_TAB || c == TK_SPC) {
            comp.p++;
            expr_paren();
            comp_tobyte();
            comp.p--;
            comp_call(c == TK_TAB ? K_PTAB : K_PSPC);
        } else if (c != ';') {
            expr_compile();
            uint8_t t = comp.type;
            comp_call(t == T_STR ? K_SPR : t == T_FLT ? K_FPR : t == T_DBL ? K_DPR : K_IPR);
            if (comp_end()) {
                comp_call(K_PNL);
                return;
            }
            continue;
        }
        if (eosnext())
            return;
    }
}

/* WRITE items: each by its type, a comma between, PR.NL after. */
static void act_write(void)
{
    for (;;) {
        if (comp_end())
            break;
        expr_compile();
        uint8_t t = comp.type;
        comp_call(t == T_STR ? K_WS : t == T_FLT ? K_WF : t == T_DBL ? K_WD : K_WI);
        if (comp_end())
            break;
        uint8_t c = *comp.p;
        if (c != ',' && c != ';')
            synerr();
        comp.p++;
        comp_call(K_WCOM);
    }
    comp_call(K_PNL);
}

/* LINE INPUT's target: the line, stored. */
static void act_lin(void)
{
    comp_call(K_SLINE);
    comp.type = T_STR;
    comp_store();
}

/* LIST [from][-[to]]: the two numbers, CALL LIST. */
static void act_list(void)
{
    const uint8_t *start = comp.p;
    uint8_t c = comp_lnum();
    lnumload();
    if (c == TK_MINUS) {
        if (eosnext())
            comp.lnum = 0xFFFF;
        else
            comp_lnum();
    } else if (comp.p == start)
        comp.lnum = 0xFFFF;
    lnumload();
    comp_call(K_LIST);
}

/* GOTO|GOSUB after ON's expression: ON kn, n 0 yet. */
static void act_on(void)
{
    comp_tobyte();
    uint8_t x = *comp.p ^ TK_GOTO;
    if (x & 0xFB) {
        comp.dg = DG_GOTO_MISSING;
        synerr();
    }
    comp_opb(O_ON, x ? 0x80 : 0);
    comp.onat = comp.out - 1;
    comp.p++;
}

/* One of ON's lines: num addr; n + 1. */
static void act_onl(void)
{
    comp_lnum();
    comp_lref();
    (*comp.onat)++;
}

static void act_goto(void)
{
    comp_op(O_GOTO);
    comp_lref();
}

static void act_gosub(void)
{
    comp_op(O_GOSUB);
    comp_lref();
}

static void act_ret(void) { comp_op(O_RETURN); }

/* CALL LOAD, SAVE or VERIFY by the token. */
static void act_fop(void) { comp_call(comp.tok + K_LOAD - TK_LOAD); }

static void act_lnum(void) { comp_lnum(); }

static void (*const actions[A_END - A_BASE])(void) = {
    [A_E - A_BASE] = expr_compile,     [A_LV - A_BASE] = expr_target,
    [A_L - A_BASE] = act_lnum,         [A_TOB - A_BASE] = comp_tobyte,
    [A_TOA - A_BASE] = comp_toaddr,    [A_TOS - A_BASE] = comp_tostring,
    [A_STR - A_BASE] = expr_string,    [A_FV - A_BASE] = act_fv,
    [A_FST - A_BASE] = comp_store,     [A_FK - A_BASE] = act_fork,
    [A_FOR - A_BASE] = act_for2,       [A_FOR1 - A_BASE] = act_for1,
    [A_NV - A_BASE] = act_nv,          [A_IF - A_BASE] = act_if,
    [A_ON - A_BASE] = act_on,          [A_ONL - A_BASE] = act_onl,
    [A_PR - A_BASE] = act_print,       [A_LST - A_BASE] = act_list,
    [A_DV - A_BASE] = act_dv,          [A_DATA - A_BASE] = act_data,
    [A_REM - A_BASE] = act_rem,        [A_LET - A_BASE] = comp_store,
    [A_IN - A_BASE] = act_in,          [A_RD - A_BASE] = act_rd,
    [A_GET - A_BASE] = act_get,        [A_GOTO - A_BASE] = act_goto,
    [A_GOSUB - A_BASE] = act_gosub,    [A_RUN - A_BASE] = act_run,
    [A_RES - A_BASE] = act_res,        [A_RET - A_BASE] = act_ret,
    [A_N0 - A_BASE] = act_n0,          [A_N1 - A_BASE] = act_n1,
    [A_NARG - A_BASE] = act_narg,      [A_FOP - A_BASE] = act_fop,
    [A_SWAP - A_BASE] = ctrl_swap,     [A_TOI - A_BASE] = comp_toint,
    [A_TOD - A_BASE] = comp_todouble,  [A_VA - A_BASE] = expr_varaddr,
    [A_WR - A_BASE] = act_write,       [A_LIN - A_BASE] = act_lin,
    [A_LE - A_BASE] = expr_assign,
};
