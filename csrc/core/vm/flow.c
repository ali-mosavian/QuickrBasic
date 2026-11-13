/* The executor's control flow (src/vm/flow.asm): lines, jumps, GOSUB, FOR and NEXT, procedures,
   ON ERROR and RESUME. */
#include "vm/flow.h"
#include "comp/blok.h"
#include "comp/comp.h"
#include "comp/link.h"
#include "io/chnl.h"
#include "io/emsg.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "mem/arry.h"
#include "mem/prog.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "num/fmth.h"
#include "vm/dbug.h"
#include "vm/evnt.h"
#include "vm/stmt.h"

static const uint8_t opsize[O_NOPS] = {
#define OP(name, h, size, fl) size,
#include "vm/ops.def"
};

static Addr lres;       /* g_lres: a FUNCTION's string result, a Desc */
static uint32_t pres;   /* a FUNCTION's INT or FLOAT result */
static Cell pres8;      /* g_pres8: a DOUBLE's */

static inline Frame *frame(Addr f) { return (Frame *)at(f); }
static inline uint8_t *cache(uint32_t off) { return at(allo.cache + off); }
static inline uint32_t ipoff(const uint8_t *p) { return addr_of(p) - allo.cache; }
static inline void tmp_clear(void) { strg.tmp_used = strg.tmp_floor; }

void flow_init(void) { lres = allo_carve(8); }

/* ex_line: the code of line exec.runln: a program line's in the cache, compiled into it if it has
   none; the direct line's compiled. exec.line set. */
static void run_line(void)
{
    uint32_t l = exec.runln;
    exec.line = l;
    const Line *ln = prog_line(l);
    if (!ln->len) {
        comp_line();
        exec.ip = flow_dcode();
    } else
        exec.ip = ln->code ? cache(ln->code) : at(link_cache(l));
}

uint8_t *flow_dcode(void)
{
    Addr a = allo.cache + DCODE;
    link.lim = allo.cache + DCODE + 1536;
    if (!link_xlate(&a))
        repl_error(E_OM);
    return at(link.code);
}

/* ex_eol: on to the next line; at the program's end, END. */
static void eol(void)
{
    uint32_t next = exec.line + prog_line(exec.line)->len;
    if (!prog_line(next)->len) {
        exec.ip = cache(0); /* CONT ends again */
        stmt_end();
    }
    exec.runln = next;
    run_line();
}

void flow_jump(uint32_t line)
{
    tmp_clear();
    if (!link.linked)
        link_start();
    link.cursor = line;
    exec.runln = line;
    run_line();
}

uint32_t flow_target(void)
{
    uint16_t num = exec_u16();
    exec.lnum = num;
    uint32_t line = exec_u32();
    if (!line) {
        if (!prog_find(num, &line))
            repl_error(E_US);
        memcpy(exec.ip - 4, &line, 4); /* found once, read after */
    }
    return line;
}

Addr flow_push(uint8_t mark)
{
    Addr f = exec.cstk;
    if (f < allo.cs_lim)
        repl_error(E_OS);
    f -= mark;
    exec.cstk = f;
    Frame *fr = frame(f);
    fr->mark = mark;
    fr->line = exec.line;
    fr->resume = ipoff(exec.ip);
    return f;
}

static void gosub(uint32_t line)
{
    flow_push(M_GS);
    flow_jump(line);
}

uint8_t *flow_place(uint32_t line, uint32_t off)
{
    if (off >= DCODE || line == DLINE)
        return cache(off);
    return flow_resolve(line, off);
}

uint8_t *flow_resolve(uint32_t line, uint32_t off)
{
    uint32_t code = prog_line(line)->code;
    if (!code)
        code = link_cache(line) - allo.cache;
    return cache(code + off);
}

/* A frame's place: exec.line, exec.ip. */
static void resume(Addr f)
{
    exec.line = frame(f)->line;
    exec.ip = flow_place(exec.line, frame(f)->resume);
}

/* GOTO num line. Undefined line number. */
void flow_goto(void)
{
    if (exec_stopchk())
        return;
    flow_jump(flow_target());
}

/* GOSUB num line. */
void flow_gosub(void)
{
    if (exec_stopchk())
        return;
    gosub(flow_target());
}

/* ON kn LineRef*n ( k -- ): kn bit 7 GOSUB; past the list for 0 or k > n. */
void flow_on(void)
{
    if (exec_stopchk())
        return;
    uint8_t k = (uint8_t)exec_popi() - 1; /* 0: 255, past any n */
    uint8_t onk = exec_u8();
    uint8_t n = onk & 0x7F;
    uint8_t *refs = exec.ip;
    exec.ip += n * sizeof(LineRef);
    if (k >= n)
        return;
    LineRef *r = (LineRef *)(refs + k * sizeof(LineRef));
    exec.lnum = r->num;
    uint32_t line = r->line;
    if (!line) {
        if (!prog_find(r->num, &line))
            repl_error(E_US);
        r->line = line;
    }
    if (onk & 0x80)
        gosub(line);
    else
        flow_jump(line);
}

/* for_find: the FOR frame of variable id above the newest other frame: *mark M_FR when found;
   else the frame met (*mark its mark), or the stack's top (*mark 1). */
static Addr for_find(uint16_t id, uint8_t *mark)
{
    Addr f = exec.cstk;
    for (; f < allo.cs_top; f += M_FR) {
        ForFrame *ff = (ForFrame *)at(f);
        *mark = ff->h.mark;
        if (*mark != M_FR || ff->id == id)
            return f;
    }
    *mark = 1;
    return f;
}

/* RETURN: the FOR frames above the GOSUB's drop. RETURN without GOSUB. */
void flow_return(void)
{
    if (exec_stopchk())
        return;
    uint8_t mark;
    Addr f = for_find(0xFFFF, &mark);
    if (mark == M_EV)
        evnt_return((EventFrame *)at(f));
    else if (mark != M_GS)
        repl_error(E_RG);
    exec.cstk = f + mark;
    tmp_clear();
    resume(f);
}

/* EOL: on to the next line. */
void flow_eol(void)
{
    if (exec_stopchk())
        return;
    flow_nextln();
}

void flow_nextln(void)
{
    tmp_clear();
    eol();
}

/* for_in: is the loop entered, its start not past its limit as its step's sign has it? */
static bool for_in(const ForFrame *ff)
{
    const Var *v = symb_var(ff->id);
    uint32_t x = rd32(v->vt);
    int32_t a, c;
    bool down;
    if ((v->info & VK_MASK) == VK_INT) {
        a = (int32_t)x;
        c = (int32_t)ff->limit;
        down = ff->step >> 31;
    } else if (ff->limit >> 16 == 0xFFFF) { /* an INT-counted FLOAT loop */
        a = (int16_t)(ff->step >> 16);
        c = (int16_t)ff->limit;
        down = ff->step & 0x8000;
    } else {
        int r = sngl_cmp(x, ff->limit);
        down = ff->step >> 31;
        return r == 0 || (r < 0 ? !down : down);
    }
    return a == c || (a < c ? !down : down);
}

/* for_mki: a FLOAT loop whose start, limit and step are whole and fit 16 bits counts in an INT:
   step's high word the count, its low the step; limit's low word the limit, its high FFFFh;
   cache the FLOAT last stored. */
static void for_mki(ForFrame *ff)
{
    int16_t s, l, v;
    if (!sngl_w16(ff->step, &s) || !sngl_w16(ff->limit, &l))
        return;
    ff->cache = rd32(symb_var(ff->id)->vt);
    if (!sngl_w16(ff->cache, &v))
        return;
    ff->step = (uint32_t)(uint16_t)v << 16 | (uint16_t)s;
    ff->limit = 0xFFFF0000 | (uint16_t)l;
}

/* jumpto: to the Jump at exec.ip: its address once found, else the target line's code (compiled
   when it has none), its statement by the map before that code, the mode's place in it. The
   address is kept in the op unless the cache was flushed meanwhile. Syntax error: no target. */
static void jumpto(void)
{
    if (exec_stopchk())
        return;
    tmp_clear();
    Jump *jp = (Jump *)exec.ip;
    Jump j = *jp;
    if (j.dest >= 3) {
        exec.line = j.line;
        exec.ip = cache(j.dest);
        return;
    }
    if (j.line == 0xFFFFFFFF)
        repl_error(E_SN);
    uint8_t flushes = link.flushes;
    exec.runln = j.line;
    run_line();
    uint32_t n = rd32(addr_of(exec.ip) - 4);
    const Stmt *e = (const Stmt *)(exec.ip - 4 - n * sizeof(Stmt));
    for (; n && e->body != j.body; e++, n--)
        ;
    if (!n)
        repl_error(E_SN);
    uint32_t code = e->code;
    if (j.dest == 1)
        code += JSIZE; /* past its JMP */
    else if (j.dest == 2) {
        if (n <= 1)
            repl_error(E_SN);
        code = e[1].code; /* the next statement's */
    }
    exec.ip += code;
    if (flushes == link.flushes)
        jp->dest = ipoff(exec.ip);
}

/* I.FOR F.FOR id ( limit step -- ), I.FOR1 F.FOR1 id ( limit -- ): the start is already stored.
   An older loop of the variable drops; a FOR ending its line goes on at the next. */
void flow_for(void)
{
    uint8_t op = *exec.opip;
    uint16_t id = exec_u16();
    uint32_t step;
    if (op < O_I_FOR1)
        step = exec_pop().u;
    else
        step = op == O_F_FOR1 ? SNGL_ONE : 1;
    uint32_t limit = exec_pop().u;
    uint8_t mark;
    Addr f = for_find(id, &mark);
    if (mark != M_FR) {
        f = exec.cstk;
        if (f < allo.cs_lim)
            repl_error(E_OS);
        f -= M_FR;
    }
    exec.cstk = f;
    ForFrame *ff = (ForFrame *)at(f);
    ff->id = id;
    ff->step = step;
    ff->limit = limit;
    ff->h.mark = M_FR;
    if (op == O_F_FOR || op == O_F_FOR1)
        for_mki(ff);
    for (;;) {
        ff = (ForFrame *)at(exec.cstk);
        if (*exec.ip == O_FSKIP) {
            if (!for_in(ff)) {
                exec.cstk += M_FR; /* not entered: past its NEXT */
                exec.ip++;
                jumpto();
                return;
            }
            exec.ip += JSIZE;
        }
        ff->h.resume = ipoff(exec.ip);
        ff->h.line = exec.line;
        if (*exec.ip != O_EOL)
            return;
        eol(); /* no next line: no return, the frame kept */
    }
}

/* for_any: step the FOR frame at f: an INT by its INT, an INT-counted FLOAT by its count, else the
   FLOAT. Illegal function call: an INT stepped out of range. */
static void for_any(Addr f)
{
    exec.cstk = f;
    ForFrame *ff = (ForFrame *)at(f);
    const Var *v = symb_var(ff->id);
    Addr cell = v->vt;
    bool more;
    if ((v->info & VK_MASK) == VK_INT) {
        int32_t x;
        if (__builtin_add_overflow((int32_t)rd32(cell), (int32_t)ff->step, &x))
            repl_error(E_IQ);
        wr32(cell, (uint32_t)x);
        more = ff->step >> 31 ? x >= (int32_t)ff->limit : x <= (int32_t)ff->limit;
    } else if (ff->limit >> 16 == 0xFFFF && rd32(cell) == ff->cache) {
        int32_t x = (int16_t)(ff->step >> 16) + (int16_t)ff->step;
        ff->step = (ff->step & 0xFFFF) | (uint32_t)x << 16;
        Sngl s = fmth_from_i32(x);
        wr32(cell, s);
        ff->cache = s;
        int32_t l = (int16_t)ff->limit;
        more = ff->step & 0x8000 ? x >= l : x <= l;
    } else {
        if (ff->limit >> 16 == 0xFFFF) { /* the body wrote it: FLOAT from here on */
            ff->limit = fmth_from_i16((int16_t)ff->limit);
            ff->step = fmth_from_i16((int16_t)ff->step);
        }
        Sngl x = fmth_add(rd32(cell), ff->step);
        wr32(cell, x);
        more = sngl_cmp(x, ff->limit) != (ff->step >> 31 ? -1 : 1);
    }
    if (more)
        resume(f);
    else
        exec.cstk += M_FR;
}

/* NEXT.V id. NEXT without FOR. */
void flow_next_v(void)
{
    if (exec_stopchk())
        return;
    tmp_clear();
    uint16_t id = exec_u16();
    Addr f = exec.cstk;
    ForFrame *ff = (ForFrame *)at(f);
    if (f < allo.cs_top && ff->h.mark == M_FR && ff->id == id) {
        for_any(f);
        return;
    }
    uint8_t mark;
    f = for_find(id, &mark);
    if (mark != M_FR)
        repl_error(E_NF);
    for_any(f);
}

/* NEXT, no variable. NEXT without FOR: no FOR frame on top. */
void flow_next(void)
{
    if (exec_stopchk())
        return;
    tmp_clear();
    Addr f = exec.cstk;
    if (f >= allo.cs_top || frame(f)->mark != M_FR)
        repl_error(E_NF);
    for_any(f);
}

/* ERR code: a compile error met running. */
void flow_err(void) { repl_error(((const ErrOp *)exec.opip)->code); }

/* ONERR num line: ON ERROR GOTO's line; 0 none. GOTO 0 in the handler raises the error it
   handles. */
void flow_onerr(void)
{
    if (!rd16(addr_of(exec.ip))) {
        exec.ip += sizeof(LineRef);
        exec.onerr = 0;
        if (exec.inerr) {
            exec.uerr = exec.err;
            repl_error(E_USR);
        }
        return;
    }
    exec.onerr = flow_target();
}

/* The innermost procedure's frame at or above f, else allo.cs_top. */
static Addr proc_frame(Addr f)
{
    while (f < allo.cs_top && frame(f)->mark != M_PC)
        f += frame(f)->mark;
    return f;
}

/* ONLERR num line: ON LOCAL ERROR GOTO's line, the running procedure's, 0 none; at the module's
   level ON ERROR's. */
void flow_onlerr(void)
{
    Addr f = proc_frame(exec.cstk);
    if (f >= allo.cs_top) {
        flow_onerr();
        return;
    }
    uint32_t line = 0;
    if (!rd16(addr_of(exec.ip)))
        exec.ip += sizeof(LineRef);
    else
        line = flow_target();
    ((ProcFrame *)at(f))->onerr = line;
}

/* The save entries' fields. */
static inline Addr se_id(Addr e) { return e; }
static inline Addr se_ref(Addr e) { return e + 2; }
static inline Addr se_val(Addr e) { return e + 4; }
static inline Addr se_cell(Addr e) { return e + 12; }

/* pr_pop: the innermost procedure's frame, and those above it, dropped as its return does: its
   parameters' cells back, the caller's locals back, a FUNCTION's result kept. Returns the frame,
   exec.cstk at it. RETURN without GOSUB: none. */
static Addr pr_pop(void)
{
    Addr f = proc_frame(exec.cstk);
    if (f >= allo.cs_top)
        repl_error(E_RG);
    exec.cstk = f;
    ProcFrame *pf = (ProcFrame *)at(f);
    for (Addr e = pf->save0; e < pf->save1; e += LSSZ) { /* an array parameter's header, dyn */
        if (rd16(se_ref(e)) == 0xFFFF)
            continue;
        uint16_t id = rd16(se_id(e));
        if (symb_var(id)->info & VI_ARR) {
            wr32(se_val(e), symb_var(id)->vt);
            wr8(se_val(e) + 4, symb_varx(id)->dyn);
        }
    }
    const Proc *p = &blok_procs[pf->proc];
    if (p->kind == PK_FUNC) { /* its result */
        uint8_t info = symb_var(p->result)->info;
        Addr c = symb_cell(p->result);
        if (info & SK_DBL)
            memcpy(&pres8, at(c), 8);
        else if ((info & VK_MASK) == VK_STR)
            flow_lsmove(c, lres);
        else
            pres = rd32(c);
    }
    for (Addr e = exec.lsp; e > pf->save1;) { /* the locals back */
        e -= LSSZ;
        uint16_t id = rd16(se_id(e));
        Var *v = symb_var(id);
        if (v->info & VI_ARR) {
            if (v->vt) {
                bool param = false; /* a parameter's array is the caller's */
                for (uint32_t i = p->npar; i-- > 0;)
                    param |= blok_params[p->par + i] == id;
                if (!param)
                    arry_free(id, 0);
            }
            v->vt = rd32(se_val(e));
            continue;
        }
        Addr c = symb_cell(id);
        if (v->info & SK_DBL)
            memcpy(at(c), at(se_val(e)), 8);
        else if ((v->info & VK_MASK) == VK_STR)
            flow_lsmove(se_val(e), c);
        else
            wr32(c, rd32(se_val(e)));
    }
    for (Addr e = pf->save0; e < pf->save1; e += LSSZ) { /* cells back, arrays to their refs */
        uint16_t ref = rd16(se_ref(e));
        if (ref == 0xFFFF)
            continue;
        if (ref == 0xFFFE) {
            symb_var(rd16(se_id(e)))->vt = rd32(se_cell(e));
            continue;
        }
        symb_var(ref)->vt = rd32(se_val(e));
        symb_varx(ref)->dyn = rd8(se_val(e) + 4);
    }
    exec.lsp = pf->save0;
    strg.tmp_used = strg.tmp_floor; /* the callee's temporaries go */
    strg.tmp_floor = pf->tmpb;
    return f;
}

uint32_t flow_handler(void)
{
    Addr h = exec.cstk;
    for (;; h += frame(h)->mark) {
        h = proc_frame(h);
        if (h >= allo.cs_top)
            return exec.onerr;
        if (((ProcFrame *)at(h))->onerr)
            break;
    }
    uint32_t handler = ((ProcFrame *)at(h))->onerr;
    for (Addr f; (f = proc_frame(exec.cstk)) != h;) { /* the procedures it called return */
        pr_pop();
        exec.cstk = f + M_PC;
        resume(f); /* the error: at the call */
        exec.ip--;
        exec.opip = exec.ip;
    }
    return handler;
}

/* ex_sbase: the data stack at the running procedure's body's start, or the module's. */
static Cell *stack_base(void)
{
    Addr f = proc_frame(exec.cstk);
    return f < allo.cs_top ? (Cell *)at(((ProcFrame *)at(f))->stk) : exec.rsp;
}

void flow_trap(uint8_t code, uint32_t handler)
{
    exec.err = code == E_USR ? exec.uerr : emsg_errnum[code];
    exec.inerr = true;
    exec.res_line = exec.line;
    exec.erl = flow_erl(exec.line);
    uint32_t left;
    const Stmt *e = flow_stmtof(exec.line, exec.opip, &left);
    exec.res_at = e->code;
    exec.res_next = left > 1 ? e[1].code : 0xFFFFFFFF;
    chnl_clear();
    exec.sp = stack_base();
    flow_jump(handler);
    if (exec.running)
        longjmp(exec.loop, 1);
    exec_run();
}

uint16_t flow_erl(uint32_t line)
{
    uint16_t erl = 0;
    for (uint32_t l = PROGBASE; l <= line && prog_line(l)->len; l += prog_line(l)->len)
        if (prog_line(l)->num != NONUM)
            erl = prog_line(l)->num;
    return erl;
}

Stmt *flow_stmtof(uint32_t line, const uint8_t *code, uint32_t *left)
{
    uint8_t *base = cache(prog_line(line)->code);
    uint32_t off = (uint32_t)(code - base);
    uint32_t n = rd32(addr_of(base) - 4);
    Stmt *e = (Stmt *)(base - 4 - n * sizeof(Stmt));
    for (; n > 1 && e[1].code <= off; e++)
        n--;
    *left = n;
    return e;
}

/* RESUME mode num line: mode 0 the failing statement again, 1 the one after it, 2 the line.
   RESUME without error (20): not in a handler. */
void flow_resume(void)
{
    if (exec_stopchk())
        return;
    if (!exec.inerr) {
        exec.uerr = 20;
        repl_error(E_USR);
    }
    exec.inerr = false;
    exec.err = 0;
    exec.sp = stack_base();
    uint8_t mode = exec_u8();
    if (mode == 2) {
        flow_jump(flow_target());
        return;
    }
    uint32_t at_ = mode ? exec.res_next : exec.res_at;
    tmp_clear();
    exec.line = exec.runln = exec.res_line;
    if (at_ == 0xFFFFFFFF)
        eol();
    else
        exec.ip = flow_resolve(exec.res_line, at_);
}

uint32_t flow_opsize(const uint8_t *op)
{
    uint32_t n = opsize[op[0]];
    if (n)
        return n;
    if (op[0] == O_ON)
        return (op[1] & 0x7F) * sizeof(LineRef) + 2;
    return op[1] + 2u;
}

void flow_setcont(void)
{
    if (exec.line == DLINE)
        return;
    exec.cont_line = exec.line;
    exec.cont_code = ipoff(exec.ip);
}

void flow_end(void)
{
    if (dbug.probing)
        dbug_done();
    chnl_closeall();
    exec.sp = exec.rsp;
    exec.cstk = allo.cs_top;
    repl_ready();
}

void flow_nodir(void)
{
    if (exec.line == DLINE)
        repl_error(E_ID);
}

/* JMP target: a block's jump; FSKIP's too. */
void flow_jmp(void) { jumpto(); }

/* X.JF target ( x -- ): jump when 0; X.JT unless 0. */
static void jump_if(bool yes)
{
    if (yes)
        jumpto();
    else
        exec.ip += JSIZE - 1;
}
void flow_i_jf(void) { jump_if(!exec_popi()); }
void flow_f_jf(void) { jump_if(!(exec_pop().u & 0x7FFFFFFF)); }
void flow_i_jt(void) { jump_if(exec_popi()); }
void flow_f_jt(void) { jump_if(exec_pop().u & 0x7FFFFFFF); }

/* FPOP: EXIT FOR's, the innermost FOR's frame dropped. */
void flow_fpop(void)
{
    Addr f = exec.cstk;
    if (f < allo.cs_top && frame(f)->mark == M_FR)
        exec.cstk = f + M_FR;
}

/* PCALL proc target ( (value ref)*n -- ): a procedure's frame. Its arguments go to save
   entries (a string copied unless a temporary's), its locals are saved there and zeroed, then
   the entries to its parameters; on at its body. A value: an INT, a FLOAT, a string's Desc, an
   array's id. A ref: FFFFh none; an array's id, PRET's store back; else a cell: the scalar
   parameter is it while the procedure runs, its value none. Out of stack space. */
void flow_pcall(void)
{
    if (exec.sp < exec.stklim || exec.cstk < allo.cs_lim)
        repl_error(E_OS);
    uint8_t *pip = exec.ip;
    const Proc *p = &blok_procs[*pip];
    Addr lim = exec.lsav + LSAVSZ - LSSZ;
    Addr pend = exec.lsp + p->npar * LSSZ;
    if (pend >= lim)
        repl_error(E_OS);
    Addr e = pend;
    for (uint32_t i = p->npar; i > 0; i--) { /* the arguments, the last first */
        e -= LSSZ;
        uint16_t id = blok_params[p->par + i - 1];
        wr16(se_id(e), id);
        uint32_t ref = exec_pop().u;
        wr16(se_ref(e), (uint16_t)ref);
        uint8_t info = symb_var(id)->info;
        if (!(info & VI_ARR) && ref != 0xFFFF) { /* a cell: the parameter's, no value */
            wr16(se_ref(e), 0);
            wr32(se_cell(e), ref);
            exec.sp++;
            continue;
        }
        Cell val = exec_pop();
        wr32(se_val(e) + 4, (uint32_t)(val.raw >> 32)); /* a DOUBLE's high half */
        if (info & VI_ARR)
            wr32(se_val(e), info == (VI_ARR | VK_INT) ? exec_record((uint16_t)val.u)
                                                      : symb_var((uint16_t)val.u)->vt);
        else if ((info & VK_MASK) == VK_STR)
            strg_set(val.a, se_val(e));
        else
            wr32(se_val(e), val.u);
    }
    Addr d = pend; /* the locals, saved and zeroed */
    for (uint16_t id = symb.first_local[*pip]; id; id = symb_varx(id)->nloc, d += LSSZ) {
        if (d >= lim)
            repl_error(E_OS);
        wr16(se_id(d), id);
        Var *v = symb_var(id);
        Addr c = symb_cell(id);
        if (v->info & VI_ARR) {
            wr32(se_val(d), v->vt);
            v->vt = 0;
        } else if (v->info & SK_DBL) {
            memcpy(at(se_val(d)), at(c), 8);
            memset(at(c), 0, 8);
        } else if ((v->info & VK_MASK) == VK_STR) {
            flow_lsmove(c, se_val(d));
            wr16(c, 0);
        } else {
            wr32(se_val(d), rd32(c));
            wr32(c, 0);
        }
    }
    for (e = exec.lsp; e < pend; e += LSSZ) { /* the arguments to the parameters */
        uint16_t id = rd16(se_id(e));
        Var *v = symb_var(id);
        if (!(v->info & VI_ARR)) { /* a scalar: vt its cell, the old one kept */
            bool cell = rd16(se_ref(e)) == 0;
            Addr c = cell ? rd32(se_cell(e)) : symb_cell(id);
            wr32(se_cell(e), v->vt);
            v->vt = c;
            wr16(se_ref(e), 0xFFFE);
            if (cell)
                continue; /* the argument's: no value */
        }
        uint32_t val = rd32(se_val(e));
        Addr c = symb_cell(id);
        if (v->info & VI_ARR) {
            v->vt = val;
            uint16_t ref = rd16(se_ref(e)); /* an array's: dynamic as its ref */
            if (ref != 0xFFFF)
                symb_varx(id)->dyn = symb_varx(ref)->dyn;
        } else if (v->info & SK_DBL)
            memcpy(at(c), at(se_val(e)), 8);
        else if ((v->info & VK_MASK) == VK_STR)
            flow_lsmove(se_val(e), c);
        else
            wr32(c, val);
    }
    Addr f = exec.cstk - M_PC; /* the frame */
    exec.cstk = f;
    ProcFrame *pf = (ProcFrame *)at(f);
    pf->h.mark = M_PC;
    pf->h.line = exec.line;
    pf->proc = *pip;
    pf->h.resume = ipoff(pip + 1 + sizeof(Jump));
    pf->save0 = exec.lsp;
    pf->save1 = pend;
    exec.lsp = d;
    pf->tmpb = strg.tmp_floor;
    strg.tmp_floor = strg.tmp_used;
    pf->stk = addr_of(exec.sp);
    pf->onerr = 0;
    exec.ip = pip + 1; /* the target */
    jumpto();
}

/* PRET: back from the procedure; a FUNCTION's result pushed. RETURN without GOSUB. */
void flow_pret(void)
{
    if (exec_stopchk())
        return;
    Addr f = pr_pop();
    const Proc *p = &blok_procs[((ProcFrame *)at(f))->proc];
    if (p->kind == PK_FUNC) {
        uint8_t info = symb_var(p->result)->info;
        if (info & SK_DBL)
            exec_push(pres8);
        else if ((info & VK_MASK) == VK_STR) { /* a new temporary */
            if (strg.tmp_used >= DSZ * NTMP)
                repl_error(E_ST);
            Addr t = strg.tmp + strg.tmp_used;
            strg.tmp_used += DSZ;
            flow_lsmove(lres, t);
            exec_pusha(t);
        } else
            exec_pusha(pres);
    }
    exec.cstk = f + M_PC;
    resume(f);
}

void flow_lsmove(Addr from, Addr to)
{
    Addr p = rd32(from + 2);
    uint16_t len = rd16(from);
    wr32(to + 2, p);
    wr16(to, len);
    if (len && p >= strg.low && p < allo.top)
        wr32(p + len + 2, to); /* the block's owner */
}
