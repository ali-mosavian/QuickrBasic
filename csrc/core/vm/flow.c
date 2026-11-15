/* Control flow: lines, jumps, GOSUB, FOR and NEXT, SUB and FUNCTION calls, ON ERROR and RESUME.
 *
 * # How it works
 * GOSUBs, events and calls are Frames on vm.frames (flow_push, flow_find). A FOR loop is none: its
 * limit and step are cells the compiler gave it, its NEXT jumps back to its body. A call sets
 * its parameters and locals aside on vm.callvars and puts them back when it ends.
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
 * | op_pcall, op_pret, op_onerr, op_onlerr, op_resume | flow_pcall, flow_pret, flow_onerr, flow_onlerr, flow_resume | MIRROR |
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

/* I.NEXT (comp/fuse.c): the counter stepped, back to the body unless past. The stop check comes
   first: a stop runs the op again, not stepped twice. */
void flow_i_next(Session *const S)
{
    if (exec_stopchk(S))
        return;

    const uint8_t *const op = S->vm.ip - 1;
    bool back;
    fault_check(&S->fault, hot_next(op, &back));
    S->vm.ip = (uint8_t *)op + NEXT_SIZE - sizeof(Jump);

    if (back)
        take_jump(&S->vm, &S->strg, &S->allo, &S->fault);
    else
        skip_jump(&S->vm);
}

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

/* A scalar's value from cell from to cell to; a string's heap block follows its descriptor. */
static void move_value(Strg *const sg, const Var *const v, const Addr from, const Addr to)
{
    if (is_double(v))
        memcpy(at(to), at(from), 8);
    else if (is_string(v))
        strg_move(sg, from, to);
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

/* Whether cell is a variable's of procedure proc. */
static bool own_cell(const Symb *const sy, const uint8_t proc, const Addr cell)
{
    const uint32_t off = cell - symb_cell(sy, 0);
    return off < sy->nvars * (uint32_t)sizeof(Cell) &&
           symb_varx(sy, (uint16_t)(off / sizeof(Cell)))->scope == proc;
}

/* Take the arguments off the data stack into callvars b, last first. Each is a ref above a value:
   a variable's cell, passed by reference and with no value; or an array argument's id. By value,
   its CallVar holds the value till bind_params; a string is copied unless a temporary. Whether
   one by reference is a variable of proc's own. */
static bool take_args(
    Vm *const vm,
    Symb *const sy,
    Arry *const ar,
    Strg *const sg,
    const uint8_t proc,
    const Proc *const p,
    CallVar *const b
)
{
    bool own = false;

    for (uint32_t i = p->npar; i-- > 0;) {
        b[i] = (CallVar){.var = p->params[i].var};

        const Var *const v = symb_var(sy, b[i].var);
        const uint32_t ref = exec_pop(vm).u;

        if (!is_array(v) && ref != NO_REF) {
            b[i].cell = ref;
            own |= own_cell(sy, proc, ref);
            vm->sp++; /* its value slot, empty */
            continue;
        }

        const Cell val = exec_pop(vm);

        if (is_array(v)) {
            b[i].arg = ref == NO_REF ? 0 : (uint16_t)ref;
            b[i].array.hdr = v->info == (VI_ARR | VK_INT) ? exec_record(ar, sy, (uint16_t)val.u)
                                                          : symb_var(sy, (uint16_t)val.u)->vt;
        } else if (is_string(v))
            strg_set(sg, val.a, addr_of(&b[i].value));
        else
            b[i].value = val;
    }

    return own;
}

/* Set the procedure's locals aside into callvars from b, emptying them; returns past the last. A
   scalar parameter is not one: bind_params keeps its own cell. */
static CallVar *save_locals(
    Vm *const vm,
    Symb *const sy,
    Strg *const sg,
    Fault *const f,
    const uint8_t proc,
    CallVar *b
)
{
    for (uint16_t id = sy->first_local[proc]; id; id = symb_varx(sy, id)->nloc) {
        Var *const v = symb_var(sy, id);

        if (!exec_saved_local(is_array(v), symb_varx(sy, id)->par))
            continue;
        if (b >= vm->callvars + MAX_CALLVARS - 1)
            fault_error(f, E_OS);

        *b = (CallVar){.var = id};

        if (is_array(v)) {
            b->array.hdr = v->vt;
            v->vt = 0;
        } else {
            move_value(sg, v, symb_cell(sy, id), addr_of(&b->value));
            clear_value(v, symb_cell(sy, id));
        }

        b++;
    }

    return b;
}

/* An argument by reference naming the procedure's own variable, the caller's: a local, set aside
   in locals..end, or a parameter by value, whose CallVar is to keep the cell's value. The argument
   is that place instead of the cell this call takes over. */
static void own_args(
    const Symb *const sy,
    const uint8_t proc,
    CallVar *const params,
    const uint32_t npar,
    const CallVar *const locals,
    const CallVar *const end
)
{
    for (CallVar *a = params; a < params + npar; a++) {
        if (!a->cell || !own_cell(sy, proc, a->cell))
            continue;

        const uint16_t id = (uint16_t)((a->cell - symb_cell(sy, 0)) / sizeof(Cell));

        for (const CallVar *b = locals; b < end; b++)
            if (b->var == id)
                a->cell = addr_of(&b->value);
        for (const CallVar *b = params; b < params + npar; b++)
            if (b->var == id && !b->cell)
                a->cell = addr_of(&b->value);
    }
}

/* Point the parameters at their arguments: a scalar by reference at the argument's cell, by value
   at its own cell given the value, the cell's value before kept in its CallVar's (tmp a string's
   on the way); an array at the argument's header. A scalar's cell before is kept in its CallVar. */
static void bind_params(Symb *const sy, Strg *const sg, CallVar *b, uint32_t n, const Addr tmp)
{
    for (; n--; b++) {
        Var *const v = symb_var(sy, b->var);

        if (is_array(v)) {
            v->vt = b->array.hdr;

            if (b->arg) /* dynamic as the argument */
                symb_varx(sy, b->var)->dyn = symb_varx(sy, b->arg)->dyn;

            continue;
        }

        const Addr arg_cell = b->cell;
        b->cell = v->vt;

        if (arg_cell) {
            v->vt = arg_cell;
            continue;
        }

        const Addr own = symb_cell(sy, b->var), value = addr_of(&b->value);
        v->vt = own;
        b->arg = CV_OWN;

        if (is_string(v)) {
            strg_move(sg, own, tmp);
            strg_move(sg, value, own);
            strg_move(sg, tmp, value);
        } else {
            const uint32_t lo = b->value.u;
            b->value.u = rd32(own);
            wr32(own, lo);

            if (is_double(v)) {
                const uint32_t hi = rd32(value + 4);
                wr32(value + 4, rd32(own + 4));
                wr32(own + 4, hi);
            }
        }
    }
}

/* PCALL proc body: call a SUB or FUNCTION, its arguments on the data stack. */
void flow_pcall(Session *const S)
{
    if (exec_stopchk(S)) /* before the arguments are taken */
        return;

    uint8_t *const call = S->vm.ip;
    const uint8_t proc = *call;
    const Proc *const p = blok_proc(&S->blok, proc);

    if (S->vm.sp < S->vm.stklim || S->vm.nframes == MAX_FRAMES ||
        S->vm.ncallvars + p->npar >= MAX_CALLVARS - 1)
        repl_error(S, E_OS);

    CallVar *const params = S->vm.callvars + S->vm.ncallvars;
    const bool own = take_args(&S->vm, &S->symb, &S->arry, &S->strg, proc, p, params);
    const CallVar *const end =
        save_locals(&S->vm, &S->symb, &S->strg, &S->fault, proc, params + p->npar);

    if (own)
        own_args(&S->symb, proc, params, p->npar, params + p->npar, end);

    bind_params(&S->symb, &S->strg, params, p->npar, S->flow.text);

    S->vm.ip = call + 1 + sizeof(Jump); /* the call returns past its body's Jump */
    flow_push(&S->vm, &S->allo, &S->fault, FRAME_CALL)->call = (Call){
        .proc = proc,
        .params = S->vm.ncallvars,
        .locals = S->vm.ncallvars + p->npar,
        .temp_floor = S->strg.tmp_floor,
        .stack = S->vm.sp,
    };

    S->vm.ncallvars = (uint32_t)(end - S->vm.callvars);
    S->strg.tmp_floor = S->strg.tmp_used;
    S->vm.ip = call + 1;
    take_jump(&S->vm, &S->strg, &S->allo, &S->fault);
}

static bool is_param(const Proc *const p, const uint16_t var)
{
    for (uint32_t i = 0; i < p->npar; i++)
        if (p->params[i].var == var)
            return true;

    return false;
}

/* End call f, the innermost, and drop the frames above it. The array arguments get their headers
   back as the callee left them, a FUNCTION's result is kept, the locals get the caller's values
   back, the parameters their cells. */
static void end_call(
    Vm *const vm,
    Symb *const sy,
    Strg *const sg,
    Arry *const ar,
    Flow *const fl,
    const Proc *const p,
    const Frame *const f
)
{
    const Call *const c = &f->call;
    CallVar *const params = vm->callvars + c->params, *const locals = vm->callvars + c->locals;
    pop_to(vm, f);

    for (CallVar *b = params; b < locals; b++)
        if (b->arg && is_array(symb_var(sy, b->var))) {
            b->array.hdr = symb_var(sy, b->var)->vt;
            b->array.dyn = symb_varx(sy, b->var)->dyn;
        }

    if (p->kind == PK_FUNC) {
        const Var *const v = symb_var(sy, p->result);
        const Addr cell = symb_cell(sy, p->result);

        if (is_double(v))
            memcpy(&fl->value, at(cell), 8);
        else if (is_string(v))
            strg_move(sg, cell, fl->text);
        else
            fl->value.u = rd32(cell);
    }

    for (CallVar *b = vm->callvars + vm->ncallvars; b-- > locals;) {
        Var *const v = symb_var(sy, b->var);

        if (!is_array(v))
            move_value(sg, v, addr_of(&b->value), symb_cell(sy, b->var));
        else {
            if (v->vt && !is_param(p, b->var)) /* a parameter's array is the caller's */
                arry_free(ar, vm, b->var, 0);

            v->vt = b->array.hdr;
        }
    }

    for (CallVar *b = params; b < locals; b++) {
        Var *const v = symb_var(sy, b->var);

        if (!is_array(v)) {
            v->vt = b->cell;

            if (b->arg == CV_OWN)
                move_value(sg, v, addr_of(&b->value), symb_cell(sy, b->var));
        } else if (b->arg) {
            symb_var(sy, b->arg)->vt = b->array.hdr;
            symb_varx(sy, b->arg)->dyn = b->array.dyn;
        }
    }

    vm->ncallvars = c->params;
    drop_temps(sg); /* the callee's */
    sg->tmp_floor = c->temp_floor;
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
    end_call(&S->vm, &S->symb, &S->strg, &S->arry, &S->flow, p, f);

    if (p->kind == PK_FUNC) {
        const Var *const v = symb_var(&S->symb, p->result);

        if (is_double(v))
            exec_push(&S->vm, S->flow.value);
        else if (is_string(v)) {
            if (S->strg.tmp_used >= DSZ * NTMP)
                repl_error(S, E_ST);

            const Addr t = S->strg.tmp + S->strg.tmp_used;
            S->strg.tmp_used += DSZ;
            strg_move(&S->strg, S->flow.text, t);
            exec_pusha(&S->vm, t);
        } else
            exec_pusha(&S->vm, S->flow.value.u);
    }

    go_to(&S->vm, &S->allo, f->ret);
}

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
        end_call(
            &S->vm,
            &S->symb,
            &S->strg,
            &S->arry,
            &S->flow,
            blok_proc(&S->blok, f->call.proc),
            f
        );
        go_to(&S->vm, &S->allo, f->ret); /* the error is now at the call */
        S->vm.opip = --S->vm.ip;
    }

    return h->call.on_error;
}

/* The data stack as the running procedure's body started, or the module's. */
static Cell *stack_base(Vm *const vm)
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
    S->vm.sp = stack_base(&S->vm);
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
    S->vm.sp = stack_base(&S->vm);
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
    repl_ready(S);
}

void flow_nodir(Session *const S)
{
    if (S->vm.line == DLINE)
        repl_error(S, E_ID);
}
