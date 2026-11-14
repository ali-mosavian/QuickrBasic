/* Control flow: lines, jumps, GOSUB, FOR and NEXT, SUB and FUNCTION calls, ON ERROR and RESUME.
 *
 * # How it works
 * GOSUBs, events, FOR loops and calls are Frames on vm.frames (flow_push, flow_find). A call sets
 * its parameters and locals aside on vm.callvars and puts them back when it ends.
 *
 * A jump op holds a Jump; its target is found on first use (flow_target). A line the cache dropped
 * is compiled again when flow_place needs its code.
 *
 * An error goes to flow_handler: the innermost call's ON LOCAL ERROR, else ON ERROR's. flow_trap
 * sets ERR, ERL and RESUME's places, then longjmps into exec_run's loop at the handler.
 *
 * # Example
 *   10 FOR i = 1 TO 3     FOR pushes a FOR frame
 *   20   PRINT i
 *   30 NEXT i             steps it, jumps back to line 20 until i is past 3
 *
 * # asm structure (src/vm/flow.asm)
 * | asm | here | |
 * |---|---|---|
 * | ex_dcode, ex_line (the direct line) | flow_dcode | MIRROR |
 * | ex_end | flow_end | MIRROR |
 * | ex_erl | flow_erl | MIRROR |
 * | ex_gsf | flow_push | D1 |
 * | ex_hnd, g_thnd | flow_handler | MIRROR |
 * | ex_trap | flow_trap | MIRROR |
 * | ex_jump | flow_jump | MIRROR |
 * | ex_nextln | flow_nextln, also NXLN's handler | MIRROR |
 * | ex_nodir | flow_nodir | MIRROR |
 * | ex_resolve | resolve, flow_place | MIRROR |
 * | ex_setcont | flow_setcont | MIRROR |
 * | ex_stmtof | flow_stmtof | MIRROR |
 * | ex_target | flow_target | MIRROR |
 * | g_lsav, LSAVSZ | vm.callvars, MAX_CALLVARS | D1 |
 * | ls_mov | flow_lsmove | MIRROR |
 * | op_size | flow_opsize | MIRROR |
 * | ex_line, ex_eol, ex_gosub, ex_jumpto, ex_sbase, for_find, for_in, for_mki, for_any, for_lp, pr_pop, err_nf, ipsave | static | MIRROR |
 * | g_pres8, g_lres | Flow (flow.h) | MIRROR |
 * | op_eol, op_goto, op_gosub, op_on, op_ret, op_for, op_nx0, op_nextv, op_err | flow_eol, flow_goto, flow_gosub, flow_on, flow_return, flow_for, flow_next, flow_next_v, flow_err | MIRROR |
 * | op_jmp, op_ijf, op_fjf, op_ijt, op_fjt, op_fpop | flow_jmp, flow_i_jf, flow_f_jf, flow_i_jt, flow_f_jt, flow_fpop | MIRROR |
 * | op_pcall, op_pret, op_onerr, op_onlerr, op_resume | flow_pcall, flow_pret, flow_onerr, flow_onlerr, flow_resume | MIRROR |
 * | stopchk | exec_stopchk | MOVED |
 *
 * - D1. Frames are typed records in vm.frames and the saved parameters and locals are CallVars; the
 *   asm keeps both as words on its control stack and save stack.
 */
#include "vm/flow.h"
#include "session.h"
#include "comp/blocks.h"
#include "comp/comp.h"
#include "comp/link.h"
#include "io/channel.h"
#include "io/errmsg.h"
#include "io/repl.h"
#include "mem/alloc.h"
#include "mem/array.h"
#include "mem/program.h"
#include "mem/string.h"
#include "mem/symbols.h"
#include "num/fpmath.h"
#include "num/single.h"
#include "vm/debug.h"
#include "vm/event.h"
#include "vm/stmts.h"

/* stopchk: the STOP check at jumps, line ends, RETURN, NEXT; true: return from the handler. */
static inline bool exec_stopchk(Session *const S) { return S->vm.stop && exec_brk(S); }

static const uint8_t op_sizes[O_NOPS] = {
#define OP(name, h, size, fl) size,
#include "vm/ops.def"
};

enum {
    DCODE_SIZE = CACHE0 - DCODE, /* the direct line's room in the cache */
};

/* A jump not yet resolved keeps its mode (JumpMode) where its code offset will go. */
enum : uint32_t { RESOLVED = TO_NEXT_STMT + 1 };
enum : uint32_t { NO_LINE = 0xFFFFFFFF }; /* a jump's line when the compiler found no target */

void flow_init(Session *const S) { S->flow.text = allo_carve(S, DSZ + 2); }

static inline uint8_t *cache_at(Session *const S, const uint32_t off)
{
    return at(S->allo.cache + off);
}
static inline uint32_t cache_off(Session *const S, const uint8_t *const p)
{
    return addr_of(p) - S->allo.cache;
}
static inline void drop_temps(Session *const S) { S->strg.tmp_used = S->strg.tmp_floor; }
static inline bool is_string(const Var *const v) { return (v->info & VK_MASK) == VK_STR; }
static inline bool is_int(const Var *const v) { return (v->info & VK_MASK) == VK_INT; }
static inline bool is_double(const Var *const v) { return v->info & SK_DBL; }
static inline bool is_array(const Var *const v) { return v->info & VI_ARR; }

/* ---- lines ---------------------------------------------------------------------------------- */

/* A line's statement map: the entries before its code, their count just before the code. */
static Stmt *stmt_map(uint8_t *const code, uint32_t *const count)
{
    *count = rd32(addr_of(code) - 4);
    return (Stmt *)(code - 4 - *count * sizeof(Stmt));
}

uint8_t *flow_dcode(Session *const S)
{
    Addr stream = S->allo.cache + DCODE;
    S->link.lim = stream + DCODE_SIZE;

    if (!link_xlate(S, &stream))
        repl_error(S, E_OM);

    return at(S->link.code);
}

/* Start running line: a program line's code from the cache, compiled into it if absent; the
   direct line compiled on its own. */
static void enter_line(Session *const S, const uint32_t line)
{
    S->vm.line = line;
    const Line *const ln = prog_line(&S->prog, line);

    if (!ln->len) {
        comp_line(S);
        S->vm.ip = flow_dcode(S);
    } else
        S->vm.ip = ln->code ? cache_at(S, ln->code) : at(link_cache(S, line));
}

static void next_line(Session *const S)
{
    const uint32_t next = S->vm.line + prog_line(&S->prog, S->vm.line)->len;

    if (!prog_line(&S->prog, next)->len) {
        S->vm.ip = cache_at(S, 0); /* CONT ends again */
        stmt_end(S);
    }

    enter_line(S, next);
}

void flow_nextln(Session *const S)
{
    drop_temps(S);
    next_line(S);
}

void flow_jump(Session *const S, const uint32_t line)
{
    drop_temps(S);

    if (!S->link.linked)
        link_start(S);

    S->link.cursor = line;
    enter_line(S, line);
}

/* The line a LineRef names, found by number on first use and kept in it. */
static uint32_t ref_line(Session *const S, LineRef *const ref)
{
    S->vm.lnum = ref->num;

    if (ref->line)
        return ref->line;

    uint32_t line;

    if (!prog_find(S, ref->num, &line))
        repl_error(S, E_US);

    return ref->line = line;
}

uint32_t flow_target(Session *const S)
{
    LineRef *const ref = (LineRef *)S->vm.ip;
    S->vm.ip += sizeof(LineRef);
    return ref_line(S, ref);
}

/* Offset off in line's code, the line compiled if the cache has none. */
static uint8_t *resolve(Session *const S, const uint32_t line, const uint32_t off)
{
    uint32_t code = prog_line(&S->prog, line)->code;

    if (!code)
        code = link_cache(S, line) - S->allo.cache;

    return cache_at(S, code + off);
}

uint8_t *flow_place(Session *const S, const Place p)
{
    if (p.code >= DCODE || p.line == DLINE)
        return cache_at(S, p.code);

    return resolve(S, p.line, p.code);
}

Place flow_here(Session *const S) { return (Place){S->vm.line, cache_off(S, S->vm.ip)}; }

static void go_to(Session *const S, const Place p)
{
    S->vm.line = p.line;
    S->vm.ip = flow_place(S, p);
}

/* ---- frames --------------------------------------------------------------------------------- */

Frame *flow_push(Session *const S, const FrameKind kind)
{
    if (S->vm.nframes == MAX_FRAMES)
        repl_error(S, E_OS);

    Frame *const f = &S->vm.frames[S->vm.nframes++];
    *f = (Frame){.kind = kind, .ret = flow_here(S)};

    return f;
}

static Frame *top_frame(Session *const S)
{
    return S->vm.nframes ? &S->vm.frames[S->vm.nframes - 1] : nullptr;
}

/* Drop f and the frames above it. */
static void pop_to(Session *const S, const Frame *const f)
{
    S->vm.nframes = (uint32_t)(f - S->vm.frames);
}

Frame *flow_find(Session *const S, const FrameKind kind)
{
    for (uint32_t i = S->vm.nframes; i-- > 0;)
        if (S->vm.frames[i].kind == kind)
            return &S->vm.frames[i];

    return nullptr;
}

/* The first frame under the FOR loops on top, nullptr none. */
static Frame *under_loops(Session *const S)
{
    for (uint32_t i = S->vm.nframes; i-- > 0;)
        if (S->vm.frames[i].kind != FRAME_FOR)
            return &S->vm.frames[i];

    return nullptr;
}

/* The loop of var among the FOR loops on top: the frame count with it on top, 0 none. */
static uint32_t find_loop(const Vm *const vm, const uint16_t var)
{
    for (uint32_t i = vm->nframes; i-- > 0 && vm->frames[i].kind == FRAME_FOR;)
        if (vm->frames[i].loop.var == var)
            return i + 1;

    return 0;
}

/* ---- GOTO, GOSUB, ON ------------------------------------------------------------------------ */

static void gosub(Session *const S, const uint32_t line)
{
    flow_push(S, FRAME_GOSUB);
    flow_jump(S, line);
}

void flow_goto(Session *const S)
{
    if (exec_stopchk(S))
        return;

    flow_jump(S, flow_target(S));
}

void flow_gosub(Session *const S)
{
    if (exec_stopchk(S))
        return;

    gosub(S, flow_target(S));
}

/* RETURN: the FOR loops inside the subroutine end too. */
void flow_return(Session *const S)
{
    if (exec_stopchk(S))
        return;

    Frame *const f = under_loops(S);

    if (!f || (f->kind != FRAME_GOSUB && f->kind != FRAME_EVENT))
        repl_error(S, E_RG);
    if (f->kind == FRAME_EVENT)
        evnt_return(S, f->event);

    pop_to(S, f);
    drop_temps(S);
    go_to(S, f->ret);
}

/* ON k GOTO/GOSUB: k counts from 1; 0 or past the list falls through. */
void flow_on(Session *const S)
{
    if (exec_stopchk(S))
        return;

    const uint8_t k = (uint8_t)exec_popi(&S->vm) - 1; /* 0 wraps past any count */
    const uint8_t kind = exec_u8(&S->vm);
    const uint8_t count = kind & ON_COUNT;
    LineRef *const lines = (LineRef *)S->vm.ip;
    S->vm.ip += count * sizeof(LineRef);

    if (k >= count)
        return;

    const uint32_t line = ref_line(S, &lines[k]);

    if (kind & ON_GOSUB)
        gosub(S, line);
    else
        flow_jump(S, line);
}

void flow_eol(Session *const S)
{
    if (exec_stopchk(S))
        return;

    flow_nextln(S);
}

/* ---- jumps ---------------------------------------------------------------------------------- */

/* The Jump operand at vm->ip. Once resolved it holds its target's code offset. Until then its
   mode picks, in the target line's code, the statement, the code past its JMP, or the next
   statement; the offset found is kept unless the cache was flushed meanwhile. */
static void take_jump(Session *const S)
{
    if (exec_stopchk(S))
        return;

    drop_temps(S);
    Jump *const op = (Jump *)S->vm.ip;
    const Jump j = *op;

    if (j.dest >= RESOLVED) {
        S->vm.line = j.line;
        S->vm.ip = cache_at(S, j.dest);
        return;
    }

    if (j.line == NO_LINE)
        repl_error(S, E_SN);

    const uint8_t flushes = S->link.flushes;
    enter_line(S, j.line);
    uint32_t left;
    const Stmt *s = stmt_map(S->vm.ip, &left);

    for (; left && s->body != j.body; s++, left--)
        ;
    if (!left)
        repl_error(S, E_SN);

    uint32_t code = s->code;

    if (j.dest == PAST_JMP)
        code += JSIZE;
    else if (j.dest == TO_NEXT_STMT) {
        if (left == 1)
            repl_error(S, E_SN);

        code = s[1].code;
    }

    S->vm.ip += code;

    if (flushes == S->link.flushes)
        op->dest = cache_off(S, S->vm.ip);
}

/* JMP: a block's jump, and FOR's skip. */
void flow_jmp(Session *const S) { take_jump(S); }

static void jump_if(Session *const S, const bool cond)
{
    if (cond)
        take_jump(S);
    else
        S->vm.ip += sizeof(Jump);
}

/* A SINGLE is true unless +0 or -0. */
static bool sngl_true(const Cell c) { return c.u & ~SNGL_SIGN; }

void flow_i_jf(Session *const S) { jump_if(S, !exec_popi(&S->vm)); }
void flow_f_jf(Session *const S) { jump_if(S, !sngl_true(exec_pop(&S->vm))); }
void flow_i_jt(Session *const S) { jump_if(S, exec_popi(&S->vm)); }
void flow_f_jt(Session *const S) { jump_if(S, sngl_true(exec_pop(&S->vm))); }

/* ---- FOR / NEXT ----------------------------------------------------------------------------- */

static bool counts_down(const ForLoop *const l) { return l->step >> 31; }

/* Is x past the loop's limit? */
static bool past_limit(const ForLoop *const l, const bool int_var, const uint32_t x)
{
    if (int_var)
        return counts_down(l) ? (int32_t)x<(int32_t)l->limit : (int32_t)x>(int32_t) l->limit;

    const int c = sngl_cmp(x, l->limit);
    return counts_down(l) ? c < 0 : c > 0;
}

/* FOR var = start TO limit [STEP step], start stored, limit and step on the data stack. A loop of
   the same var still running restarts. A loop not entered goes past its NEXT. */
void flow_for(Session *const S)
{
    const uint8_t op = *S->vm.opip;
    const uint16_t var = exec_u16(&S->vm);
    const uint32_t step =
        op == O_I_FOR || op == O_F_FOR ? exec_pop(&S->vm).u : (op == O_F_FOR1 ? SNGL_ONE : 1);
    const ForLoop loop = {var, exec_pop(&S->vm).u, step};

    const uint32_t old = find_loop(&S->vm, var);

    if (old)
        S->vm.nframes = old - 1;
    if (*S->vm.ip == O_FSKIP) {
        const Var *const v = symb_var(&S->symb, var);

        if (past_limit(&loop, is_int(v), rd32(v->vt))) {
            S->vm.ip++;
            take_jump(S);
            return;
        }

        S->vm.ip += JSIZE;
    }

    while (*S->vm.ip == O_EOL) /* the body starts on a later line */
        next_line(S);

    flow_push(S, FRAME_FOR)->loop = loop;
}

/* Step the loop on top of depth frames; back to its body until it is done. An INT stepped past
   its range is Illegal function call. */
static void step_loop(Session *const S, const uint32_t depth)
{
    const Frame *const f = &S->vm.frames[depth - 1];
    const ForLoop *const l = &f->loop;
    const Var *const v = symb_var(&S->symb, l->var);
    const Addr cell = v->vt;
    const bool int_var = is_int(v);
    uint32_t x;

    if (int_var) {
        int32_t sum;

        if (__builtin_add_overflow((int32_t)rd32(cell), (int32_t)l->step, &sum))
            repl_error(S, E_IQ);

        x = (uint32_t)sum;
    } else
        x = fmth_add(rd32(cell), l->step);

    wr32(cell, x);

    if (past_limit(l, int_var, x)) {
        S->vm.nframes = depth - 1;
        return;
    }

    S->vm.nframes = depth; /* the loops inside it end */
    go_to(S, f->ret);
}

/* NEXT var. */
void flow_next_v(Session *const S)
{
    if (exec_stopchk(S))
        return;

    drop_temps(S);
    const uint32_t depth = find_loop(&S->vm, exec_u16(&S->vm));

    if (!depth)
        repl_error(S, E_NF);

    step_loop(S, depth);
}

/* NEXT: the innermost loop. */
void flow_next(Session *const S)
{
    if (exec_stopchk(S))
        return;

    drop_temps(S);
    const uint32_t depth = S->vm.nframes;

    if (!depth || S->vm.frames[depth - 1].kind != FRAME_FOR)
        repl_error(S, E_NF);

    step_loop(S, depth);
}

/* EXIT FOR. */
void flow_fpop(Session *const S)
{
    const Frame *const f = top_frame(S);

    if (f && f->kind == FRAME_FOR)
        pop_to(S, f);
}

/* ---- SUB and FUNCTION ----------------------------------------------------------------------- */

/* A scalar's value from cell from to cell to; a string's heap block follows its descriptor. */
static void move_value(Session *const S, const Var *const v, const Addr from, const Addr to)
{
    if (is_double(v))
        memcpy(at(to), at(from), 8);
    else if (is_string(v))
        flow_lsmove(S, from, to);
    else
        wr32(to, rd32(from));
}

static void clear_value(const Var *const v, const Addr cell)
{
    if (is_double(v))
        memset(at(cell), 0, 8);
    else if (is_string(v))
        wr16(cell, 0);
    else
        wr32(cell, 0);
}

enum : uint32_t { NO_REF = 0xFFFF }; /* an argument's ref: passed by value, or no array */

/* Take the arguments off the data stack into callvars b, last first. Each is a ref above a value:
   a variable's cell, passed by reference and with no value; or an array argument's id. By value,
   its CallVar holds the value till bind_params; a string is copied unless a temporary. */
static void take_args(Session *const S, const Proc *const p, CallVar *const b)
{
    for (uint32_t i = p->npar; i-- > 0;) {
        b[i] = (CallVar){.var = p->params[i].var};

        const Var *const v = symb_var(&S->symb, b[i].var);
        const uint32_t ref = exec_pop(&S->vm).u;

        if (!is_array(v) && ref != NO_REF) {
            b[i].cell = ref;
            S->vm.sp++; /* its value slot, empty */
            continue;
        }

        const Cell val = exec_pop(&S->vm);

        if (is_array(v)) {
            b[i].arg = ref == NO_REF ? 0 : (uint16_t)ref;
            b[i].array.hdr = v->info == (VI_ARR | VK_INT) ? exec_record(S, (uint16_t)val.u)
                                                          : symb_var(&S->symb, (uint16_t)val.u)->vt;
        } else if (is_string(v))
            strg_set(S, val.a, addr_of(&b[i].value));
        else
            b[i].value = val;
    }
}

/* Set the procedure's locals aside into callvars from b, emptying them; returns past the last. */
static CallVar *save_locals(Session *const S, const uint8_t proc, CallVar *b)
{
    for (uint16_t id = S->symb.first_local[proc]; id; id = symb_varx(&S->symb, id)->nloc, b++) {
        if (b >= S->vm.callvars + MAX_CALLVARS - 1)
            repl_error(S, E_OS);

        *b = (CallVar){.var = id};

        Var *const v = symb_var(&S->symb, id);

        if (is_array(v)) {
            b->array.hdr = v->vt;
            v->vt = 0;
        } else {
            move_value(S, v, symb_cell(&S->symb, id), addr_of(&b->value));
            clear_value(v, symb_cell(&S->symb, id));
        }
    }

    return b;
}

/* Point the parameters at their arguments: a scalar by reference at the argument's cell, by value
   at its own cell given the value; an array at the argument's header. A scalar's cell before is
   kept in its CallVar. */
static void bind_params(Session *const S, CallVar *b, uint32_t n)
{
    for (; n--; b++) {
        Var *const v = symb_var(&S->symb, b->var);

        if (is_array(v)) {
            v->vt = b->array.hdr;

            if (b->arg) /* dynamic as the argument */
                symb_varx(&S->symb, b->var)->dyn = symb_varx(&S->symb, b->arg)->dyn;

            continue;
        }

        const Addr arg_cell = b->cell;
        b->cell = v->vt;

        if (arg_cell)
            v->vt = arg_cell;
        else {
            v->vt = symb_cell(&S->symb, b->var);
            move_value(S, v, addr_of(&b->value), v->vt);
        }
    }
}

/* PCALL proc body: call a SUB or FUNCTION, its arguments on the data stack. */
void flow_pcall(Session *const S)
{
    uint8_t *const call = S->vm.ip;
    const uint8_t proc = *call;
    const Proc *const p = blok_proc(&S->blok, proc);

    if (S->vm.sp < S->vm.stklim || S->vm.nframes == MAX_FRAMES ||
        S->vm.ncallvars + p->npar >= MAX_CALLVARS - 1)
        repl_error(S, E_OS);

    CallVar *const params = S->vm.callvars + S->vm.ncallvars;
    take_args(S, p, params);
    const CallVar *const end = save_locals(S, proc, params + p->npar);
    bind_params(S, params, p->npar);

    S->vm.ip = call + 1 + sizeof(Jump); /* the call returns past its body's Jump */
    flow_push(S, FRAME_CALL)->call = (Call){
        .proc = proc,
        .params = S->vm.ncallvars,
        .locals = S->vm.ncallvars + p->npar,
        .temp_floor = S->strg.tmp_floor,
        .stack = S->vm.sp,
    };

    S->vm.ncallvars = (uint32_t)(end - S->vm.callvars);
    S->strg.tmp_floor = S->strg.tmp_used;
    S->vm.ip = call + 1;
    take_jump(S);
}

static bool is_param(Session *const S, const Proc *const p, const uint16_t var)
{
    for (uint32_t i = 0; i < p->npar; i++)
        if (p->params[i].var == var)
            return true;

    return false;
}

/* End call f, the innermost, and drop the frames above it. The array arguments get their headers
   back as the callee left them, a FUNCTION's result is kept, the locals get the caller's values
   back, the parameters their cells. */
static void end_call(Session *const S, const Frame *const f)
{
    const Call *const c = &f->call;
    const Proc *const p = blok_proc(&S->blok, c->proc);
    CallVar *const params = S->vm.callvars + c->params, *const locals = S->vm.callvars + c->locals;
    pop_to(S, f);

    for (CallVar *b = params; b < locals; b++)
        if (b->arg) {
            b->array.hdr = symb_var(&S->symb, b->var)->vt;
            b->array.dyn = symb_varx(&S->symb, b->var)->dyn;
        }

    if (p->kind == PK_FUNC) {
        const Var *const v = symb_var(&S->symb, p->result);
        const Addr cell = symb_cell(&S->symb, p->result);

        if (is_double(v))
            memcpy(&S->flow.value, at(cell), 8);
        else if (is_string(v))
            flow_lsmove(S, cell, S->flow.text);
        else
            S->flow.value.u = rd32(cell);
    }

    for (CallVar *b = S->vm.callvars + S->vm.ncallvars; b-- > locals;) {
        Var *const v = symb_var(&S->symb, b->var);

        if (!is_array(v))
            move_value(S, v, addr_of(&b->value), symb_cell(&S->symb, b->var));
        else {
            if (v->vt && !is_param(S, p, b->var)) /* a parameter's array is the caller's */
                arry_free(S, b->var, 0);

            v->vt = b->array.hdr;
        }
    }

    for (CallVar *b = params; b < locals; b++) {
        if (!is_array(symb_var(&S->symb, b->var)))
            symb_var(&S->symb, b->var)->vt = b->cell;
        else if (b->arg) {
            symb_var(&S->symb, b->arg)->vt = b->array.hdr;
            symb_varx(&S->symb, b->arg)->dyn = b->array.dyn;
        }
    }

    S->vm.ncallvars = c->params;
    drop_temps(S); /* the callee's */
    S->strg.tmp_floor = c->temp_floor;
}

/* PRET: return from the call, a FUNCTION's result pushed. */
void flow_pret(Session *const S)
{
    if (exec_stopchk(S))
        return;

    const Frame *const f = flow_find(S, FRAME_CALL);

    if (!f)
        repl_error(S, E_RG);

    end_call(S, f);
    const Proc *const p = blok_proc(&S->blok, f->call.proc);

    if (p->kind == PK_FUNC) {
        const Var *const v = symb_var(&S->symb, p->result);

        if (is_double(v))
            exec_push(&S->vm, S->flow.value);
        else if (is_string(v)) {
            if (S->strg.tmp_used >= DSZ * NTMP)
                repl_error(S, E_ST);

            const Addr t = S->strg.tmp + S->strg.tmp_used;
            S->strg.tmp_used += DSZ;
            flow_lsmove(S, S->flow.text, t);
            exec_pusha(&S->vm, t);
        } else
            exec_pusha(&S->vm, S->flow.value.u);
    }

    go_to(S, f->ret);
}

void flow_lsmove(Session *const S, const Addr from, const Addr to)
{
    const Desc d = *(Desc *)at(from);
    *(Desc *)at(to) = d;

    if (d.len && d.ptr >= S->strg.low && d.ptr < S->allo.top)
        ((StrTail *)at(d.ptr + d.len))->owner = to;
}

/* ---- ON ERROR / RESUME ---------------------------------------------------------------------- */

/* ON [LOCAL] ERROR GOTO's line, 0 for GOTO 0. */
static uint32_t handler_line(Session *const S)
{
    if (((LineRef *)S->vm.ip)->num)
        return flow_target(S);

    S->vm.ip += sizeof(LineRef);
    return 0;
}

/* ON ERROR GOTO. GOTO 0 inside the handler raises the error being handled. */
void flow_onerr(Session *const S)
{
    S->vm.err.handler = handler_line(S);

    if (!S->vm.err.handler && S->vm.err.active) {
        S->vm.err.user = S->vm.err.code;
        repl_error(S, E_USR);
    }
}

/* ON LOCAL ERROR GOTO: the running procedure's handler; in the module, ON ERROR's. */
void flow_onlerr(Session *const S)
{
    Frame *const f = flow_find(S, FRAME_CALL);

    if (f)
        f->call.on_error = handler_line(S);
    else
        flow_onerr(S);
}

uint32_t flow_handler(Session *const S)
{
    Frame *h = nullptr;

    for (uint32_t i = S->vm.nframes; i-- > 0 && !h;)
        if (S->vm.frames[i].kind == FRAME_CALL && S->vm.frames[i].call.on_error)
            h = &S->vm.frames[i];
    if (!h)
        return S->vm.err.handler;
    for (const Frame *f; (f = flow_find(S, FRAME_CALL)) != h;) {
        end_call(S, f);
        go_to(S, f->ret); /* the error is now at the call */
        S->vm.opip = --S->vm.ip;
    }

    return h->call.on_error;
}

/* The data stack as the running procedure's body started, or the module's. */
static Cell *stack_base(Session *const S)
{
    const Frame *const f = flow_find(S, FRAME_CALL);
    return f ? f->call.stack : S->vm.rsp;
}

void flow_trap(Session *const S, const uint8_t code, const uint32_t handler)
{
    S->vm.err.code = code == E_USR ? S->vm.err.user : emsg_errnum[code];
    S->vm.err.active = true;
    S->vm.err.line = flow_erl(S, S->vm.line);
    uint32_t left;
    const Stmt *const s = flow_stmtof(S, S->vm.line, S->vm.opip, &left);
    S->vm.err.failed = (Place){S->vm.line, s->code};

    S->vm.err.next = (Place){S->vm.line, left > 1 ? s[1].code : LINE_END};

    chnl_clear(S);
    S->vm.sp = stack_base(S);
    flow_jump(S, handler);

    if (S->vm.running)
        longjmp(S->vm.loop, 1);

    exec_run(S);
}

uint16_t flow_erl(Session *const S, const uint32_t line)
{
    uint16_t erl = 0;

    for (uint32_t l = PROGBASE; l <= line && prog_line(&S->prog, l)->len;

         l += prog_line(&S->prog, l)->len)
        if (prog_line(&S->prog, l)->num != NONUM)
            erl = prog_line(&S->prog, l)->num;
    return erl;
}

Stmt *flow_stmtof(
    Session *const S,
    const uint32_t line,
    const uint8_t *const code,
    uint32_t *const left
)
{
    uint8_t *const base = cache_at(S, prog_line(&S->prog, line)->code);
    const uint32_t off = (uint32_t)(code - base);
    Stmt *s = stmt_map(base, left);

    for (; *left > 1 && s[1].code <= off; s++)
        (*left)--;

    return s;
}

/* RESUME, RESUME NEXT, RESUME line. */
void flow_resume(Session *const S)
{
    if (exec_stopchk(S))
        return;
    if (!S->vm.err.active) {
        repl_qberr(S, QB_RESUME_NO_ERROR);
    }

    S->vm.err.active = false;
    S->vm.err.code = 0;
    S->vm.sp = stack_base(S);
    const ResumeMode mode = exec_u8(&S->vm);

    if (mode == RESUME_LINE) {
        flow_jump(S, flow_target(S));
        return;
    }

    const Place p = mode == RESUME_NEXT ? S->vm.err.next : S->vm.err.failed;
    drop_temps(S);
    S->vm.line = p.line;

    if (p.code == LINE_END)
        next_line(S);
    else
        S->vm.ip = resolve(S, p.line, p.code);
}

/* A compile error, raised when its statement runs. */
void flow_err(Session *const S) { repl_error(S, ((const ErrOp *)S->vm.opip)->code); }

/* ---- the rest ------------------------------------------------------------------------------- */

uint32_t flow_opsize(const uint8_t *const op)
{
    if (op_sizes[op[0]])
        return op_sizes[op[0]];
    if (op[0] == O_ON)
        return (op[1] & ON_COUNT) * sizeof(LineRef) + 2;

    return op[1] + 2u; /* its length byte */
}

void flow_setcont(Session *const S)
{
    if (S->vm.line != DLINE)
        S->vm.cont = flow_here(S);
}

void flow_end(Session *const S)
{
    if (S->dbug.probing)
        dbug_done(S);

    chnl_closeall(S);
    S->vm.sp = S->vm.rsp;
    S->vm.nframes = 0;
    repl_ready(S);
}

void flow_nodir(Session *const S)
{
    if (S->vm.line == DLINE)
        repl_error(S, E_ID);
}
