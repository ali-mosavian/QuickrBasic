/* Control flow: lines, jumps, GOSUB, FOR and NEXT, SUB and FUNCTION calls, ON ERROR and RESUME.
 *
 * # How it works
 * GOSUBs, events and calls are Frames on vm.frames (flow_push, flow_find). A FOR loop is none: its
 * limit and step are cells the compiler gave it, its NEXT jumps back to its body. A call's frame
 * is on the data stack: its arguments' addresses above vm.fp, its locals below; exec_run runs
 * PCALL and PRET inline. Only array parameters and locals are set aside on vm.callvars.
 *
 * A jump op holds a Jump, a GOTO a LineRef; link_resolve set both before the run. Every line's
 * code is in the cache while linked (link_start).
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
 * | ex_resolve | flow_place | MIRROR |
 * | ex_setcont | flow_setcont | MIRROR |
 * | ex_stmtof | flow_stmtof | MIRROR |
 * | ex_target | flow_target | MIRROR |
 * | g_lsav, LSAVSZ | vm.callvars, MAX_CALLVARS | D1 |
 * | ls_mov | strg_move (mem/string.c) | MOVED |
 * | op_size | flow_opsize | MIRROR |
 * | ex_line, ex_eol, ex_gosub, ex_jumpto, ex_sbase, pr_pop, err_nf, ipsave | static | MIRROR |
 * | g_pres8, g_lres | Flow (flow.h) | MIRROR |
 * | op_eol, op_goto, op_gosub, op_on, op_ret, op_err | flow_eol, flow_goto, flow_gosub, flow_on, flow_return, flow_err | MIRROR |
 * | op_for, op_nx0, op_nextv, for_find, for_in, for_mki, for_any, for_lp | flow_i_past, flow_f_past | D2 |
 * | op_jmp, op_ijf, op_fjf, op_ijt, op_fjt, op_fpop | flow_jmp, flow_i_jf, flow_f_jf, flow_i_jt, flow_f_jt, flow_fpop | MIRROR |
 * | op_pcall, op_pret | flow_pcall, flow_pret | DIVERGES: frames on the data stack |
 * | op_onerr, op_onlerr, op_resume | flow_onerr, flow_onlerr, flow_resume | MIRROR |
 * | stopchk | exec_stopchk | MOVED |
 *
 * - D1. Frames are typed records in vm.frames and the saved parameters and locals are CallVars; the
 *   asm keeps both as words on its control stack and save stack.
 * - D2. FOR and NEXT compile to loads, stores, a compare and a jump (comp/control.c); PAST is the
 *   compare of a step whose sign is known only when run.
 */
#include "vm/flow.h"
#include "vm/hot.h"
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

void flow_init(Session *const S) { S->flow.text = allo_carve(&S->allo, DSZ + 2); }

static inline uint8_t *cache_at(const Allo *const al, const uint32_t off)
{
    return at(al->cache + off);
}
static inline uint32_t cache_off(const Allo *const al, const uint8_t *const p)
{
    return addr_of(p) - al->cache;
}
static inline void drop_temps(Strg *const sg) { sg->tmp_used = sg->tmp_floor; }
static inline bool is_string(const Var *const v) { return (v->info & VK_MASK) == VK_STR; }
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

    if (!link_xlate(&S->comp, &stream))
        repl_error(S, E_OM);

    S->link.nmiss = 0;
    link_resolve(&S->comp, DLINE, S->link.code - S->allo.cache);
    link_check(&S->comp);
    return at(S->link.code);
}

/* The direct line compiled on its own. Cold: off the hot path. */
[[gnu::cold, gnu::noinline]]
static uint8_t *direct_compiled(Session *const S)
{
    comp_line(&S->comp);
    return flow_dcode(S);
}

void flow_nextln(Session *const S)
{
    uint8_t *const code = flow_next_code(&S->vm, &S->strg, &S->prog, &S->allo);

    if (code) {
        S->vm.ip = code;
        return;
    }

    drop_temps(&S->strg);
    const uint32_t next = S->vm.line + prog_line(&S->prog, S->vm.line)->len;

    if (!prog_line(&S->prog, next)->len) {
        S->vm.ip = cache_at(&S->allo, 0); /* CONT ends again */
        stmt_end(S);
    }

    S->vm.line = next;
    const Line *const ln = prog_line(&S->prog, next); /* len 0: the direct line, compiled */
    S->vm.ip = ln->len ? cache_at(&S->allo, ln->code) : direct_compiled(S);
}

void flow_jump(Session *const S, const uint32_t line)
{
    drop_temps(&S->strg);

    if (!S->link.linked)
        link_start(&S->comp);

    S->link.cursor = line;
    S->vm.line = line;
    const Line *const ln = prog_line(&S->prog, line); /* len 0: the direct line, compiled */
    S->vm.ip = ln->len ? cache_at(&S->allo, ln->code) : direct_compiled(S);
}

uint32_t flow_target(Vm *const vm)
{
    const LineRef *const ref = (const LineRef *)vm->ip;
    vm->ip += sizeof(LineRef);
    return ref->line;
}

uint8_t *flow_place(const Allo *const al, const Place p) { return cache_at(al, p.code); }

Place flow_here(const Vm *const vm, const Allo *const al)
{
    return (Place){vm->line, cache_off(al, vm->ip)};
}

static void go_to(Vm *const vm, const Allo *const al, const Place p)
{
    vm->line = p.line;
    vm->ip = flow_place(al, p);
}

/* ---- frames --------------------------------------------------------------------------------- */

Frame *flow_push(Vm *const vm, const Allo *const al, Fault *const f, const FrameKind kind)
{
    if (vm->nframes == MAX_FRAMES)
        fault_error(f, E_OS);

    Frame *const fr = &vm->frames[vm->nframes++];
    *fr = (Frame){.kind = kind, .ret = flow_here(vm, al)};

    return fr;
}

static Frame *top_frame(Vm *const vm)
{
    return vm->nframes ? &vm->frames[vm->nframes - 1] : nullptr;
}

/* Drop f and the frames above it. */
static void pop_to(Vm *const vm, const Frame *const f) { vm->nframes = (uint32_t)(f - vm->frames); }

Frame *flow_find(Vm *const vm, const FrameKind kind)
{
    for (uint32_t i = vm->nframes; i-- > 0;)
        if (vm->frames[i].kind == kind)
            return &vm->frames[i];

    return nullptr;
}

/* ---- GOTO, GOSUB, ON ------------------------------------------------------------------------ */

void flow_goto(Session *const S)
{
    if (exec_stopchk(S))
        return;

    flow_jump(S, flow_target(&S->vm));
}

void flow_gosub(Session *const S)
{
    if (exec_stopchk(S))
        return;

    const uint32_t line = flow_target(&S->vm);
    flow_push(&S->vm, &S->allo, &S->fault, FRAME_GOSUB);
    flow_jump(S, line);
}

/* RETURN. */
void flow_return(Session *const S)
{
    if (exec_stopchk(S))
        return;

    Frame *const f = top_frame(&S->vm);

    if (!f || (f->kind != FRAME_GOSUB && f->kind != FRAME_EVENT))
        repl_error(S, E_RG);
    if (f->kind == FRAME_EVENT)
        evnt_return(&S->evnt, &S->vm, f->event);

    pop_to(&S->vm, f);
    drop_temps(&S->strg);
    go_to(&S->vm, &S->allo, f->ret);
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

    const uint32_t line = lines[k].line;

    if (kind & ON_GOSUB)
        flow_push(&S->vm, &S->allo, &S->fault, FRAME_GOSUB);

    flow_jump(S, line);
}

void flow_eol(Session *const S)
{
    if (exec_stopchk(S))
        return;

    flow_nextln(S);
}

/* ---- jumps ---------------------------------------------------------------------------------- */

/* The Jump operand at vm->ip taken. A direct line's block jump has none: Syntax error. The stop
   check is the caller's. */
static void take_jump(Vm *const vm, Strg *const sg, const Allo *const al, Fault *const f)
{
    uint8_t *const code = flow_jump_code(vm, sg, al, vm->ip);

    if (!code)
        fault_error(f, E_SN);

    vm->ip = code;
}

static void skip_jump(Vm *const vm) { vm->ip += sizeof(Jump); }

/* JMP: a block's jump, and FOR's skip. */
void flow_jmp(Session *const S)
{
    if (!exec_stopchk(S))
        take_jump(&S->vm, &S->strg, &S->allo, &S->fault);
}

/* A SINGLE is true unless +0 or -0. */
static bool sngl_true(const Cell c) { return c.u & ~SNGL_SIGN; }

static void branch(Session *const S, const bool taken)
{
    if (taken)
        take_jump(&S->vm, &S->strg, &S->allo, &S->fault);
    else
        skip_jump(&S->vm);
}

/* The stop check comes before the pop: a stop runs the op again. */
void flow_i_jf(Session *const S)
{
    if (!exec_stopchk(S))
        branch(S, !exec_popi(&S->vm));
}
void flow_f_jf(Session *const S)
{
    if (!exec_stopchk(S))
        branch(S, !sngl_true(exec_pop(&S->vm)));
}
void flow_i_jt(Session *const S)
{
    if (!exec_stopchk(S))
        branch(S, exec_popi(&S->vm));
}
void flow_f_jt(Session *const S)
{
    if (!exec_stopchk(S))
        branch(S, sngl_true(exec_pop(&S->vm)));
}

/* ---- FOR / NEXT ----------------------------------------------------------------------------- */

/* I.NEXT.UP, I.NEXT.DN (comp/fuse.c): the counter stepped, back to the body unless past. The stop
   check comes first: a stop runs the op again, not stepped twice. */
static void next(Session *const S, const bool up)
{
    if (exec_stopchk(S))
        return;

    const uint8_t *const op = S->vm.ip - 1;
    bool back;
    fault_check(&S->fault, hot_next(op, up, &back));
    S->vm.ip = (uint8_t *)op + NEXT_SIZE - sizeof(Jump);
    branch(S, back);
}

void flow_i_next_up(Session *const S) { next(S, true); }
void flow_i_next_dn(Session *const S) { next(S, false); }

/* I.JFC ... (comp/fuse.c): an INT compare, then JF's jump unless it is true. */
#define X(name, src)                                                                               \
    void flow_##name(Session *const S)                                                             \
    {                                                                                              \
        if (exec_stopchk(S))                                                                       \
            return;                                                                                \
                                                                                                   \
        Regs r = {S->vm.ip, S->vm.sp, S->vm.fp};                                                   \
        const bool yes = hot_jfc(&r, src);                                                         \
        S->vm.ip = r.ip;                                                                           \
        S->vm.sp = r.sp;                                                                           \
        branch(S, !yes);                                                                           \
    }
HOT_JFCS(X)
#undef X

/* Is x past limit, counting by step: INTs, or SINGLEs' bits? */
static bool past_limit(const bool ints, const uint32_t x, const uint32_t limit, const uint32_t step)
{
    const bool down = step >> 31;

    if (ints)
        return down ? (int32_t)x<(int32_t)limit : (int32_t)x>(int32_t) limit;

    const int c = sngl_cmp(x, limit);
    return down ? c < 0 : c > 0;
}

/* PAST ( x limit step -- flag ): a loop whose step's sign is known only when run; -1 when x is
   past limit. */
static void past(Vm *const vm, const bool ints)
{
    const uint32_t step = exec_pop(vm).u;
    const uint32_t limit = exec_pop(vm).u;
    Cell *const x = vm->sp;
    x->i = past_limit(ints, x->u, limit, step) ? -1 : 0;
}
void flow_i_past(Session *const S) { past(&S->vm, true); }
void flow_f_past(Session *const S) { past(&S->vm, false); }

/* D.PAST ( x limit step -- flag ): PAST's of DOUBLEs. */
void flow_d_past(Session *const S)
{
    const double step = exec_popd(&S->vm);
    const double limit = exec_popd(&S->vm);
    Cell *const x = S->vm.sp;
    x->i = (step < 0 ? x->d < limit : x->d > limit) ? -1 : 0;
}

/* ---- SUB and FUNCTION ----------------------------------------------------------------------- */

/* A call's frame on the data stack: each argument a value cell, then its ref cell above it (the
   address its parameter is); vm.fp the last argument's ref cell, the locals ENTER made below it. An
   array or record argument's cells hold the argument's id instead: it and the procedure's local
   arrays are bound through Var.vt, set aside in vm.callvars (PF_ARRAYS, link's). */

/* Parameter i's ref cell, of npar, in the frame at fp. */
static Cell *param_ref(const Addr fp, const uint32_t npar, const uint32_t i)
{
    return (Cell *)at(fp) + 2 * (npar - 1 - i);
}

/* Bind the array parameters to their arguments' headers and set the local arrays aside, emptied,
   into callvars from b; returns past the last. The parameters' CallVars come first: *locals is
   where the locals' start. */
static CallVar *bind_arrays(
    Vm *const vm,
    Symb *const sy,
    Arry *const ar,
    Fault *const f,
    const uint8_t proc,
    const Proc *const p,
    CallVar *b,
    uint32_t *const locals
)
{
    CallVar *const end = vm->callvars + MAX_CALLVARS - 1;
    CallVar *const params = b;

    for (uint32_t i = 0; i < p->npar; i++) { /* the headers first: an argument may be a local */
        const uint16_t id = p->params[i].var;
        const Var *const v = symb_var(sy, id);

        if (!is_array(v))
            continue;
        if (b >= end)
            fault_error(f, E_OS);

        const Cell *const ref = param_ref(vm->fp, p->npar, i);
        const uint16_t arg = (uint16_t)ref[1].u;
        *b = (CallVar){.var = id, .arg = ref->u == FLOW_NO_REF ? 0 : (uint16_t)ref->u};

        b->array.hdr = v->info == VI_RECARR ? exec_record(ar, sy, arg) : symb_var(sy, arg)->vt;
        b++;
    }

    *locals = (uint32_t)(b - vm->callvars);

    for (uint16_t id = sy->first_local[proc]; id; id = symb_varx(sy, id)->nloc) {
        Var *const v = symb_var(sy, id);

        if (!is_array(v))
            continue;
        if (b >= end)
            fault_error(f, E_OS);

        *b++ = (CallVar){.var = id, .array.hdr = v->vt};

        v->vt = 0;
    }

    for (CallVar *c = params; c < vm->callvars + *locals; c++) {
        symb_var(sy, c->var)->vt = c->array.hdr;

        if (c->arg) /* dynamic as the argument */
            symb_varx(sy, c->var)->dyn = symb_varx(sy, c->arg)->dyn;
    }

    return b;
}

/* PCALL proc n body: the frame at the arguments on the data stack, its n locals' cells below it
   zeroed, its frame pushed, into the body. */
void flow_pcall(Session *const S)
{
    if (exec_stopchk(S)) /* before the arguments are taken */
        return;

    Vm *const vm = &S->vm;
    uint8_t *const call = vm->ip;
    const uint8_t proc = *call;
    const Proc *const p = blok_proc(&S->blok, proc);

    const uint32_t n = load16(call + 1);

    if (vm->sp < vm->stklim || (uint32_t)(vm->sp - vm->stklim) < n || vm->nframes == MAX_FRAMES)
        repl_error(S, E_OS);

    const Addr caller = vm->fp;
    const uint32_t first = vm->ncallvars;
    uint32_t locals = first;
    vm->fp = addr_of(vm->sp);

    if (p->flags & PF_ARRAYS) {
        CallVar *const end =
            bind_arrays(vm, &S->symb, &S->arry, &S->fault, proc, p, vm->callvars + first, &locals);
        vm->ncallvars = (uint32_t)(end - vm->callvars);
    }

    vm->sp -= n;
    memset(vm->sp, 0, n * sizeof(Cell));
    vm->frames[vm->nframes++] = (Frame){
        .kind = FRAME_CALL,
        .ret = {vm->line, cache_off(&S->allo, call + 4 + sizeof(Jump))},
        .call = {
            .proc = proc,
            .params = first,
            .locals = locals,
            .temp_floor = S->strg.tmp_floor,
            .stack = vm->sp,
            .fp = vm->fp,
            .npar = p->npar,
            .caller = caller,
        },
    };

    S->strg.tmp_floor = S->strg.tmp_used;
    vm->ip = call + 4;
    take_jump(vm, &S->strg, &S->allo, &S->fault);
}

void flow_pcall_a(Session *const S) { flow_pcall(S); }

static bool is_param(const Proc *const p, const uint16_t var)
{
    for (uint32_t i = 0; i < p->npar; i++)
        if (p->params[i].var == var)
            return true;

    return false;
}

/* End call f, the innermost, and drop the frames above it: the array arguments get their headers
   back as the callee left them, the local arrays the caller's; its frame popped, its arguments
   with it. */
static void end_call(
    Vm *const vm,
    Symb *const sy,
    Strg *const sg,
    Arry *const ar,
    const Proc *const p,
    const Frame *const f
)
{
    const Call c = f->call;
    CallVar *const params = vm->callvars + c.params, *const locals = vm->callvars + c.locals;
    pop_to(vm, f);

    for (CallVar *b = params; b < locals; b++) {
        b->array.hdr = symb_var(sy, b->var)->vt;
        b->array.dyn = symb_varx(sy, b->var)->dyn;
    }

    for (CallVar *b = vm->callvars + vm->ncallvars; b-- > locals;) {
        Var *const v = symb_var(sy, b->var);

        if (v->vt && !is_param(p, b->var)) /* a parameter's array is the caller's */
            arry_free(ar, vm, b->var, 0);

        v->vt = b->array.hdr;
    }

    for (CallVar *b = params; b < locals; b++)
        if (b->arg) {
            symb_var(sy, b->arg)->vt = b->array.hdr;
            symb_varx(sy, b->arg)->dyn = b->array.dyn;
        }

    vm->ncallvars = c.params;
    vm->sp = (Cell *)at(c.fp) + 2 * c.npar;
    vm->fp = c.caller;
    drop_temps(sg); /* the callee's */
    sg->tmp_floor = c.temp_floor;
}

/* PRET: return from the call, a FUNCTION's result pushed. */
void flow_pret(Session *const S)
{
    if (exec_stopchk(S))
        return;

    const Frame *const f = flow_find(&S->vm, FRAME_CALL);

    if (!f)
        repl_error(S, E_RG);

    const Proc *const p = blok_proc(&S->blok, f->call.proc);
    const bool fn = p->kind == PK_FUNC;
    const bool text = fn && is_string(symb_var(&S->symb, p->result));
    Cell result = {};

    if (fn) { /* out of its frame before the frame goes */
        const Addr cell = flow_cell(&S->symb, f->call.fp, p->result);

        if (text)
            strg_move(&S->strg, cell, S->flow.text);
        else
            memcpy(&result, at(cell), sizeof(Cell));
    }

    const Place back = f->ret;
    end_call(&S->vm, &S->symb, &S->strg, &S->arry, p, f);
    go_to(&S->vm, &S->allo, back);

    if (!fn)
        return;

    if (!text) {
        exec_push(&S->vm, result);
        return;
    }

    if (S->strg.tmp_used >= DSZ * NTMP)
        repl_error(S, E_ST);

    const Addr t = S->strg.tmp + S->strg.tmp_used;
    S->strg.tmp_used += DSZ;
    strg_move(&S->strg, S->flow.text, t);
    exec_pusha(&S->vm, t);
}

void flow_pret_fv(Session *const S) { flow_pret(S); }
void flow_pret_fl(Session *const S) { flow_pret(S); }
void flow_pret_x(Session *const S) { flow_pret(S); }

/* ---- ON ERROR / RESUME ---------------------------------------------------------------------- */

/* ON [LOCAL] ERROR GOTO's line, 0 for GOTO 0. */
static uint32_t handler_line(Vm *const vm)
{
    if (((LineRef *)vm->ip)->num)
        return flow_target(vm);

    vm->ip += sizeof(LineRef);
    return 0;
}

/* ON ERROR GOTO. GOTO 0 inside the handler raises the error being handled. */
void flow_onerr(Session *const S)
{
    S->vm.err.handler = handler_line(&S->vm);

    if (!S->vm.err.handler && S->vm.err.active) {
        S->vm.err.user = S->vm.err.code;
        repl_error(S, E_USR);
    }
}

/* ON LOCAL ERROR GOTO: the running procedure's handler; in the module, ON ERROR's. */
void flow_onlerr(Session *const S)
{
    Frame *const f = flow_find(&S->vm, FRAME_CALL);

    if (f)
        f->call.on_error = handler_line(&S->vm);
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
    for (const Frame *f; (f = flow_find(&S->vm, FRAME_CALL)) != h;) {
        const Place back = f->ret;
        end_call(&S->vm, &S->symb, &S->strg, &S->arry, blok_proc(&S->blok, f->call.proc), f);
        go_to(&S->vm, &S->allo, back); /* the error is now at the call */
        S->vm.opip = --S->vm.ip;
    }

    return h->call.on_error;
}

/* The data stack as the running procedure's body started, or the module's. */
Cell *flow_stack_base(Vm *const vm)
{
    const Frame *const f = flow_find(vm, FRAME_CALL);
    return f ? f->call.stack : vm->rsp;
}

void flow_trap(Session *const S, const uint8_t code, const uint32_t handler)
{
    S->vm.err.code = code == E_USR ? S->vm.err.user : emsg_errnum[code];
    S->vm.err.active = true;
    S->vm.err.line = flow_erl(&S->prog, S->vm.line);
    uint32_t left;
    const Stmt *const s = flow_stmtof(&S->prog, &S->allo, S->vm.line, S->vm.opip, &left);
    const uint32_t base = prog_line(&S->prog, S->vm.line)->code; /* the map's offsets are from it */
    S->vm.err.failed = (Place){S->vm.line, base + s->code};

    S->vm.err.next = (Place){S->vm.line, left > 1 ? base + s[1].code : LINE_END};

    chnl_clear(&S->chnl);
    S->vm.sp = flow_stack_base(&S->vm);
    flow_jump(S, handler);

    if (S->vm.running)
        longjmp(S->vm.loop, 1);

    exec_run(S);
}

uint16_t flow_erl(Prog *const pg, const uint32_t line)
{
    uint16_t erl = 0;

    for (uint32_t l = PROGBASE; l <= line && prog_line(pg, l)->len;

         l += prog_line(pg, l)->len)
        if (prog_line(pg, l)->num != NONUM)
            erl = prog_line(pg, l)->num;
    return erl;
}

Stmt *flow_stmtof(
    const Prog *const pg,
    const Allo *const al,
    const uint32_t line,
    const uint8_t *const code,
    uint32_t *const left
)
{
    uint8_t *const base = cache_at(al, prog_line(pg, line)->code);
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
        fault_qberr(&S->fault, QB_RESUME_NO_ERROR);
    }

    S->vm.err.active = false;
    S->vm.err.code = 0;
    S->vm.sp = flow_stack_base(&S->vm);
    const ResumeMode mode = exec_u8(&S->vm);

    if (mode == RESUME_LINE) {
        flow_jump(S, flow_target(&S->vm));
        return;
    }

    const Place p = mode == RESUME_NEXT ? S->vm.err.next : S->vm.err.failed;
    drop_temps(&S->strg);
    S->vm.line = p.line;

    if (p.code == LINE_END)
        flow_nextln(S);
    else
        S->vm.ip = flow_place(&S->allo, p);
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

void flow_setcont(Vm *const vm, const Allo *const al)
{
    if (vm->line != DLINE)
        vm->cont = flow_here(vm, al);
}

void flow_end(Session *const S)
{
    if (S->dbug.probing)
        dbug_done(S);

    fault_check(&S->fault, chnl_closeall(&S->chnl));
    S->vm.sp = S->vm.rsp;
    S->vm.nframes = 0;
    S->vm.fp = 0;
    repl_ready(S);
}

void flow_nodir(Session *const S)
{
    if (S->vm.line == DLINE)
        repl_error(S, E_ID);
}
