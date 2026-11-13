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
uint8_t comp_xbuf[XLIM + COMP_TAIL];
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

void comp_byte(const uint8_t b)
{
    if (comp.out >= comp.lim)
        comp_fail(E_OM); /* cm_om: the code is full */
    *comp.out++ = b;
}

void comp_opb(const uint8_t op, const uint8_t b)
{
    comp_op(op);
    comp_byte(b);
}

void comp_word(const uint16_t w)
{
    comp_byte((uint8_t)w);
    comp_byte((uint8_t)(w >> 8));
}

void comp_opw(const uint8_t op, const uint16_t w)
{
    comp.lnum = w; /* the asm's wv: the word */
    comp_op(op);
    comp_word(w);
}

void comp_dword(const uint32_t d)
{
    comp_word((uint16_t)d);
    comp_word((uint16_t)(d >> 16));
}

void comp_lref(void)
{
    comp_word(comp.lnum);
    comp_dword(comp.jline);
}

/* A mark into the code, at or past where: moved up a byte with it. Null is below any. */
static void follow(uint8_t **const start, uint8_t **const end, const uint8_t *const where)
{
    if ((uintptr_t)*start >= (uintptr_t)where) {
        ++*start;
        ++*end;
    }
}

void comp_insert(const uint8_t b, uint8_t *const where)
{
    if (comp.out >= comp.lim)
        comp_fail(E_OM);
    memmove(where + 1, where, (size_t)(comp.out - where));
    comp.out++;
    *where = b;
    follow(&comp.iload, &comp.iloadend, where);
    follow(&comp.cmp, &comp.cmpend, where);
}

void comp_int(const uint32_t n)
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

void comp_call(const uint8_t k) { comp_opb(O_CALL, k); }

/* cm_vop: X.LD.V or X.ST.V of a scalar parameter of the procedure compiled: V.LD, V.ST id kind
   instead (kind 0 4 bytes, 1 8, 2 a Desc). */
static bool parop(const uint8_t op, const uint16_t id)
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

void comp_opid(const uint8_t op, const uint16_t id)
{
    if (!parop(op, id))
        comp_opw(op, id);
}

uint8_t *comp_intleaf(void)
{
    if (comp.out != comp.iloadend)
        return nullptr;
    const uint8_t op = *comp.iload;
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
    const uint8_t op = comp.type == T_DBL ? O_D_TOI : O_F_TOI;
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
static void tonarrow(const uint8_t fop, const uint8_t iop)
{
    if (comp.type & T_STR)
        mismatch();
    if (comp.type == T_DBL)
        comp_toint();
    const bool is_int = comp.type == T_INT;
    if (is_int) {
        const uint8_t *const leaf = comp_intleaf();
        if (leaf && *leaf == O_I_LD_U8)
            return;
    }
    comp.type = T_INT;
    comp_op(is_int ? iop : fop);
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
    uint8_t *const leaf = comp_intleaf();
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
    const Sngl f = *leaf == O_I_LD_I32 ? fmth_from_i32((int32_t)load32(leaf + 1))
                                 : fmth_from_u16(load16(leaf + 1));
    comp.out = leaf; /* F.LD.F32 in its place */
    comp_op(O_F_LD_F32);
    comp_dword(f);
}

void comp_toinfo(const uint8_t info)
{
    if (info & SK_DBL)
        comp_todouble();
    else
        switch (info & VK_MASK) {
        case VK_INT: comp_toint(); break;
        case VK_FLT: comp_tofloat(); break;
        default: comp_tostring();
        }
}

void comp_target(const uint16_t id)
{
    comp.tgt = id;
    comp.tgtinfo = symb_var(id)->info;
    comp.tgtsubs = 0;
}

/* The target's store by its subscripts: none, one, or many and their count. */
static void store_to(const uint8_t scalar, const uint8_t one, const uint8_t many)
{
    const uint8_t subs = comp.tgtsubs;
    if (subs > 1) {
        comp_opid(many, comp.tgt);
        comp_byte(subs);
    } else
        comp_opid(subs ? one : scalar, comp.tgt);
}

/* The value converted for the target; its store ops' offset from the INT's: 0, 1 FLOAT, 2 STRING. */
static uint8_t convert_for_target(void)
{
    switch (comp.tgtinfo & VK_MASK) {
    case VK_INT: /* a SHORT: Overflow past 16 bits, unless it is one */
        expr_shclass();
        comp_toint();
        if (symb_var(comp.tgt)->type == ET_SH && !expr.shclass)
            comp_op(O_I_CHK16);
        return 0;
    case VK_FLT:
        comp_tofloat();
        return 1;
    case VK_STR: { /* a STRING * n: n chars */
        comp_tostring();
        const uint16_t fix = scop_fix(symb_var(comp.tgt)->name, comp.tgtsubs != 0);
        if (fix) {
            comp_int(fix);
            comp_call(K_SFIX);
        }
        return 2;
    }
    default:
        synerr();
    }
}

void comp_store(void)
{
    if (comp.tgtsubs == TGT_FIELD) {
        ctrl_recstore();
        return;
    }
    if (comp.tgtinfo & SK_DBL) {
        comp_todouble();
        store_to(O_D_ST_V, O_D_ST_A1, O_D_ST_A);
        return;
    }
    const uint8_t k = convert_for_target();
    store_to(O_I_ST_V + k, O_I_ST_A1 + k, O_I_ST_A + k);
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

bool comp_eos(const uint8_t **const p)
{
    const uint8_t c = comp_peek(p);
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

void comp_need(const uint8_t b)
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
static void lfind(const uint16_t key, const uint16_t kind)
{
    const uint16_t scope = comp.modlabel ? 0 : scop_own();
    comp.modlabel = false;
    comp.jline = scop_lfind(scope | kind, key);
}

uint8_t comp_lnum(void)
{
    comp.jline = 0;
    const uint8_t c = comp_cur();
    if (c == M_VAR) {
        lfind(comp_nameof(comp.p), 0);
        comp.lnum = NONUM;
        comp.p = comp_skipname(comp.p);
        return comp_cur();
    }
    if (c != M_INT) {
        comp.modlabel = false;
        comp.lnum = 0;
        return c;
    }
    const uint32_t n = load32(comp.p + 1);
    if (n >= LNUM_LIMIT)
        synerr();
    comp.lnum = (uint16_t)n;
    comp.p += INT_TOKEN_SIZE;
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

uint8_t comp_suffix(const uint8_t c)
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

bool comp_ispar(const uint16_t id)
{
    if (symb_var(id)->info & VI_ARR || !comp.proc)
        return false;
    const Proc *const p = &blok_procs[comp.proc];
    for (uint32_t i = 0; i < p->npar; i++)
        if (blok_params[p->par + i] == id)
            return true;
    return false;
}

bool comp_param(const uint16_t name, const uint8_t array, uint16_t *const id)
{
    if (!comp.proc)
        return false;
    const Proc *const p = &blok_procs[comp.proc];
    for (uint32_t i = 0; i < p->npar; i++) {
        const uint16_t v = blok_params[p->par + i];
        const Var *const var = symb_var(v);
        if (var->name == name && !((var->info ^ array) & VI_ARR)) {
            *id = v;
            return true;
        }
    }
    return false;
}

void comp_scope(const uint16_t name)
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
static void declared(const uint16_t name)
{
    if (!decl.explicit_ || symb_name(name)->flags & (NM_SUB | NM_FUNC | NM_FN))
        return;
    if (!scop_known(name))
        repl_error(E_VND);
}

uint16_t comp_var(const uint8_t shape, const uint8_t et)
{
    const uint8_t *const q = comp.p;
    const uint8_t first = q[2];
    const uint8_t sfx = comp_suffix(comp_suffixof(q));
    const uint16_t name = comp_nameof(q);
    comp.p = comp_skipname(q);
    uint8_t kind = shape > 1 || (!shape && *comp.p == '(') ? VI_ARR : 0;
    uint8_t type = et;
    if (!type && sfx)
        type = scop_sfx(name, sfx, kind != 0);
    else if (!type) {
        uint16_t id;
        if (comp_param(name, kind, &id)) /* no suffix: a parameter's own type */
            return id;
        type = scop_type(name, kind != 0);
        if (!type)
            type = symb.deft[upcase(first) - 'A'];
    }
    declared(name);
    if (type >= ET_REC) { /* a record: ctrl_recarray's, recvar's kind */
        if (!kind)
            kind = VI_RECARR;
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

static void mapstmt(const uint32_t bodyoff)
{
    uint8_t *const e = comp.map - (comp.nstmt + 1) * sizeof(Stmt);
    if (e < comp.out + sizeof(Stmt)) { /* no room: the code is full */
        comp.lim = comp.out;
        return;
    }
    *(Stmt *)e = (Stmt){.body = bodyoff, .code = (uint32_t)(comp.out - comp.buf)}; /* packed: any address */
    comp.lim = e - COMP_TAIL;
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

void comp_fail(const uint8_t err)
{
    uint8_t *const o = comp.out; /* ERR err dg at:4 state:2 */
    o[0] = O_ERR;
    o[1] = err;
    o[2] = comp.dg;
    store32(o + 3, (uint32_t)(comp.p - comp.body));
    store16(o + 7, comp.dfa);
    comp.out = o + 9;
    comp.dg = 0;
    comp.dfa = 0xFFFF;
    done();
}

static uint32_t pslook(const uint32_t st, const uint8_t cls)
{
    for (uint32_t i = lexr_ps_first[st]; i < lexr_ps_first[st + 1]; i++)
        if (lexr_ps_xclass[i] == cls)
            return lexr_ps_xentry[i];
    return lexr_ps_default[st];
}

static void (*const actions[A_END - A_BASE])(void);

void comp_dfa(const uint16_t state)
{
    uint32_t st = state;
    for (;;) {
        const uint32_t e = pslook(st, lexr_ps_class[comp_cur()]);
        const uint8_t code = (uint8_t)e;
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

void comp_expected(const uint16_t state)
{
    const char *names[PS_NC * 2];
    const uint8_t n = comp_expnames(state, names);
    for (uint8_t i = 0; i < n; i++) {
        cons_puts(names[i]);
        const uint8_t left = n - i - 1;
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
    const uint8_t c = *comp.p;
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

/* cm_nx: the statement compiled, its end next. */
static void ended(void)
{
    dbgdrop();
    if (!comp_end())
        synerr();
}

/* The statements after comp.p, to the line's end. */
static void stmts(void)
{
    for (;;) { /* cm_new: comp.p before a statement */
        comp.then = false;
        if (!eosnext()) {
            token();
            ended();
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
            ended();
        }
    }
}

void comp_text(const uint8_t *const body)
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
    comp.map = comp.buf + XLIM + COMP_TAIL; /* the map grows down from the top */
    comp.lim = comp.map - COMP_TAIL;
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

/* The one of four for the value's type. */
static uint8_t by_type(const uint8_t i, const uint8_t f, const uint8_t d, const uint8_t s)
{
    return comp_pick(comp.type, i, f, d, s);
}

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
    else /* comp_dtest made a DOUBLE an INT */
        comp_op(by_type(O_I_IF, O_F_IF, O_I_IF, O_S_IF));
    comp.then = true;
    const uint8_t kw = *comp.p; /* THEN or GOTO */
    c = comp_curn();
    if (kw == TK_GOTO || c == M_INT)
        comp_dfa(ST_GOTO);
    else
        token();
}

/* FOR's variable: the target of the start's store. */
static void act_fv(void)
{
    const uint16_t id = comp_name();
    const uint8_t info = symb_var(id)->info;
    if (info & VI_ARR)
        synerr();
    comp.forvar = id;
    comp_target(id);
    if (info & SK_DBL) /* a DOUBLE's FOR: not yet */
        mismatch();
    const uint8_t kind = info & VK_MASK;
    if (kind == VK_STR)
        mismatch();
    comp.forkind = kind;
}

/* The value as FOR's variable wants it. */
static void act_fork(void) { comp_toinfo(comp.forkind); }

static void act_for(const uint8_t op)
{
    comp_opid(comp.forkind ? op : op + 1, comp.forvar); /* F.FOR, F.FOR1 after the INT's */
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

/* DATA: no code, READ finds the text in the body. */
static void act_data(void) { comp.p = comp_skipdata(comp.kw + 1); }

/* REM: the line's end. */
static void act_rem(void) { done(); }

/* RUN, RESTORE [line]: CALL, or the line and CALL .L. */
static void runres(const uint8_t k)
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
static void dim1(const uint16_t id)
{
    const uint32_t flags = (comp.isstatic ? DIM_STATIC : 0) | comp.redim;
    const uint32_t n = dimsubs();
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
    if (p[1] == TK_X) { /* AS SHORT: AS, FFh n */
        comp.p = p + 3;
        return;
    }
    comp.p = p + 2;
    if (p[1] == TK_STRING && p[2] == TK_MUL) /* STRING * n */
        comp.p += 1 + INT_TOKEN_SIZE;
}

/* A DIM item: the subscripts, its id, n, CALL DIM; a scalar STRING * n: n spaces. */
static void act_dv(void)
{
    uint16_t id = comp_name();
    if (*comp.p == '(') {
        const uint16_t name = symb_var(id)->name;
        const uint8_t t = scop_rec(name, true); /* a record array: its own variable */
        if (t)
            id = ctrl_recarray(name, t);
        dim1(id);
        dimas();
        return;
    }
    dimas();
    const uint16_t fix = scop_fix(symb_var(id)->name, false);
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
    const uint8_t info = comp.tgtinfo;
    if (info & SK_DBL && k != K_FGET) {
        k = k == K_FIN ? K_DIN : K_DREAD;
        comp.type = T_DBL;
    } else if ((info & VK_MASK) == VK_STR) {
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
        comp_call(by_type(K_USI, K_USF, K_USD, K_USS));
        if (comp_end())
            break;
        const uint8_t c = *comp.p;
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
        const uint8_t c = *comp.p;
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
            comp_call(by_type(K_IPR, K_FPR, K_DPR, K_SPR));
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
        comp_call(by_type(K_WI, K_WF, K_WD, K_WS));
        if (comp_end())
            break;
        const uint8_t c = *comp.p;
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
    const uint8_t *const start = comp.p;
    const uint8_t c = comp_lnum();
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
    const uint8_t c = *comp.p;
    if (c != TK_GOTO && c != TK_GOSUB) {
        comp.dg = DG_GOTO_MISSING;
        synerr();
    }
    comp_opb(O_ON, c == TK_GOSUB ? ON_GOSUB : 0);
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
