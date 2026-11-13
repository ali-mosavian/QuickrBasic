/* Control flow: lines, jumps, GOSUB, FOR and NEXT, SUB and FUNCTION calls, ON ERROR and RESUME. */
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
#include "num/sngl.h"
#include "vm/dbug.h"
#include "vm/evnt.h"
#include "vm/stmt.h"

static const uint8_t op_sizes[O_NOPS] = {
#define OP(name, h, size, fl) size,
#include "vm/ops.def"
};

enum {
    DCODE_SIZE = CACHE0 - DCODE, /* the direct line's room in the cache */
    ON_GOSUB = 0x80,             /* ON's count byte: GOSUB, not GOTO */
    ON_COUNT = 0x7F,
    RESUME_WITHOUT_ERROR = 20,
};

/* A jump not yet resolved keeps its mode (JumpMode) where its code offset will go. */
enum : uint32_t { RESOLVED = TO_NEXT_STMT + 1 };
enum : uint32_t { NO_LINE = 0xFFFFFFFF }; /* a jump's line when the compiler found no target */

/* A FUNCTION's result between its return and its push. A string's descriptor is in the arena,
   where a heap block's owner must be. */
static struct {
    Addr text;
    Cell value;
} result;

void flow_init(void) { result.text = allo_carve(DSZ + 2); }

static inline uint8_t *cache_at(const uint32_t off) { return at(allo.cache + off); }
static inline uint32_t cache_off(const uint8_t *const p) { return addr_of(p) - allo.cache; }
static inline void drop_temps(void) { strg.tmp_used = strg.tmp_floor; }
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

uint8_t *flow_dcode(void)
{
    Addr stream = allo.cache + DCODE;
    link.lim = stream + DCODE_SIZE;
    if (!link_xlate(&stream))
        repl_error(E_OM);
    return at(link.code);
}

/* Start running line: a program line's code from the cache, compiled into it if absent; the
   direct line compiled on its own. */
static void enter_line(Vm *const vm, const uint32_t line)
{
    vm->line = line;
    const Line *const ln = prog_line(line);
    if (!ln->len) {
        comp_line();
        vm->ip = flow_dcode();
    } else
        vm->ip = ln->code ? cache_at(ln->code) : at(link_cache(line));
}

static void next_line(Vm *const vm)
{
    const uint32_t next = vm->line + prog_line(vm->line)->len;
    if (!prog_line(next)->len) {
        vm->ip = cache_at(0); /* CONT ends again */
        stmt_end(vm);
    }
    enter_line(vm, next);
}

void flow_nextln(Vm *const vm)
{
    drop_temps();
    next_line(vm);
}

void flow_jump(Vm *const vm, const uint32_t line)
{
    drop_temps();
    if (!link.linked)
        link_start();
    link.cursor = line;
    enter_line(vm, line);
}

/* The line a LineRef names, found by number on first use and kept in it. */
static uint32_t ref_line(Vm *const vm, LineRef *const ref)
{
    vm->lnum = ref->num;
    if (ref->line)
        return ref->line;
    uint32_t line;
    if (!prog_find(ref->num, &line))
        repl_error(E_US);
    return ref->line = line;
}

uint32_t flow_target(Vm *const vm)
{
    LineRef *const ref = (LineRef *)vm->ip;
    vm->ip += sizeof(LineRef);
    return ref_line(vm, ref);
}

/* Offset off in line's code, the line compiled if the cache has none. */
static uint8_t *resolve(const uint32_t line, const uint32_t off)
{
    uint32_t code = prog_line(line)->code;
    if (!code)
        code = link_cache(line) - allo.cache;
    return cache_at(code + off);
}

uint8_t *flow_place(const Place p)
{
    if (p.code >= DCODE || p.line == DLINE)
        return cache_at(p.code);
    return resolve(p.line, p.code);
}

Place flow_here(const Vm *const vm) { return (Place){vm->line, cache_off(vm->ip)}; }

static void go_to(Vm *const vm, const Place p)
{
    vm->line = p.line;
    vm->ip = flow_place(p);
}

/* ---- frames --------------------------------------------------------------------------------- */

Frame *flow_push(Vm *const vm, const FrameKind kind)
{
    if (vm->nframes == MAX_FRAMES)
        repl_error(E_OS);
    Frame *const f = &vm->frames[vm->nframes++];
    *f = (Frame){.kind = kind, .ret = flow_here(vm)};
    return f;
}

static Frame *top_frame(Vm *const vm) { return vm->nframes ? &vm->frames[vm->nframes - 1] : nullptr; }

/* Drop f and the frames above it. */
static void pop_to(Vm *const vm, const Frame *const f) { vm->nframes = (uint32_t)(f - vm->frames); }

Frame *flow_find(Vm *const vm, const FrameKind kind)
{
    for (uint32_t i = vm->nframes; i-- > 0;)
        if (vm->frames[i].kind == kind)
            return &vm->frames[i];
    return nullptr;
}

/* The first frame under the FOR loops on top, nullptr none. */
static Frame *under_loops(Vm *const vm)
{
    for (uint32_t i = vm->nframes; i-- > 0;)
        if (vm->frames[i].kind != FRAME_FOR)
            return &vm->frames[i];
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

static void gosub(Vm *const vm, const uint32_t line)
{
    flow_push(vm, FRAME_GOSUB);
    flow_jump(vm, line);
}

void flow_goto(Vm *const vm)
{
    if (exec_stopchk(vm))
        return;
    flow_jump(vm, flow_target(vm));
}

void flow_gosub(Vm *const vm)
{
    if (exec_stopchk(vm))
        return;
    gosub(vm, flow_target(vm));
}

/* RETURN: the FOR loops inside the subroutine end too. */
void flow_return(Vm *const vm)
{
    if (exec_stopchk(vm))
        return;
    Frame *const f = under_loops(vm);
    if (!f || (f->kind != FRAME_GOSUB && f->kind != FRAME_EVENT))
        repl_error(E_RG);
    if (f->kind == FRAME_EVENT)
        evnt_return(vm, f->event);
    pop_to(vm, f);
    drop_temps();
    go_to(vm, f->ret);
}

/* ON k GOTO/GOSUB: k counts from 1; 0 or past the list falls through. */
void flow_on(Vm *const vm)
{
    if (exec_stopchk(vm))
        return;
    const uint8_t k = (uint8_t)exec_popi(vm) - 1; /* 0 wraps past any count */
    const uint8_t kind = exec_u8(vm);
    const uint8_t count = kind & ON_COUNT;
    LineRef *const lines = (LineRef *)vm->ip;
    vm->ip += count * sizeof(LineRef);
    if (k >= count)
        return;
    const uint32_t line = ref_line(vm, &lines[k]);
    if (kind & ON_GOSUB)
        gosub(vm, line);
    else
        flow_jump(vm, line);
}

void flow_eol(Vm *const vm)
{
    if (exec_stopchk(vm))
        return;
    flow_nextln(vm);
}

/* ---- jumps ---------------------------------------------------------------------------------- */

/* The Jump operand at vm->ip. Once resolved it holds its target's code offset. Until then its
   mode picks, in the target line's code, the statement, the code past its JMP, or the next
   statement; the offset found is kept unless the cache was flushed meanwhile. */
static void take_jump(Vm *const vm)
{
    if (exec_stopchk(vm))
        return;
    drop_temps();
    Jump *const op = (Jump *)vm->ip;
    const Jump j = *op;
    if (j.dest >= RESOLVED) {
        vm->line = j.line;
        vm->ip = cache_at(j.dest);
        return;
    }
    if (j.line == NO_LINE)
        repl_error(E_SN);
    const uint8_t flushes = link.flushes;
    enter_line(vm, j.line);
    uint32_t left;
    const Stmt *s = stmt_map(vm->ip, &left);
    for (; left && s->body != j.body; s++, left--)
        ;
    if (!left)
        repl_error(E_SN);
    uint32_t code = s->code;
    if (j.dest == PAST_JMP)
        code += JSIZE;
    else if (j.dest == TO_NEXT_STMT) {
        if (left == 1)
            repl_error(E_SN);
        code = s[1].code;
    }
    vm->ip += code;
    if (flushes == link.flushes)
        op->dest = cache_off(vm->ip);
}

/* JMP: a block's jump, and FOR's skip. */
void flow_jmp(Vm *const vm) { take_jump(vm); }

static void jump_if(Vm *const vm, const bool cond)
{
    if (cond)
        take_jump(vm);
    else
        vm->ip += sizeof(Jump);
}

/* A SINGLE is true unless +0 or -0. */
static bool sngl_true(const Cell c) { return c.u & ~SNGL_SIGN; }

void flow_i_jf(Vm *const vm) { jump_if(vm, !exec_popi(vm)); }
void flow_f_jf(Vm *const vm) { jump_if(vm, !sngl_true(exec_pop(vm))); }
void flow_i_jt(Vm *const vm) { jump_if(vm, exec_popi(vm)); }
void flow_f_jt(Vm *const vm) { jump_if(vm, sngl_true(exec_pop(vm))); }

/* ---- FOR / NEXT ----------------------------------------------------------------------------- */

static bool counts_down(const ForLoop *const l) { return l->step >> 31; }

/* Is x past the loop's limit? */
static bool past_limit(const ForLoop *const l, const bool int_var, const uint32_t x)
{
    if (int_var)
        return counts_down(l) ? (int32_t)x < (int32_t)l->limit : (int32_t)x > (int32_t)l->limit;
    const int c = sngl_cmp(x, l->limit);
    return counts_down(l) ? c < 0 : c > 0;
}

/* FOR var = start TO limit [STEP step], start stored, limit and step on the data stack. A loop of
   the same var still running restarts. A loop not entered goes past its NEXT. */
void flow_for(Vm *const vm)
{
    const uint8_t op = *vm->opip;
    const uint16_t var = exec_u16(vm);
    const uint32_t step = op == O_I_FOR || op == O_F_FOR ? exec_pop(vm).u
                                                   : (op == O_F_FOR1 ? SNGL_ONE : 1);
    const ForLoop loop = {var, exec_pop(vm).u, step};

    const uint32_t old = find_loop(vm, var);
    if (old)
        vm->nframes = old - 1;
    if (*vm->ip == O_FSKIP) {
        const Var *const v = symb_var(var);
        if (past_limit(&loop, is_int(v), rd32(v->vt))) {
            vm->ip++;
            take_jump(vm);
            return;
        }
        vm->ip += JSIZE;
    }
    while (*vm->ip == O_EOL) /* the body starts on a later line */
        next_line(vm);
    flow_push(vm, FRAME_FOR)->loop = loop;
}

/* Step the loop on top of depth frames; back to its body until it is done. An INT stepped past
   its range is Illegal function call. */
static void step_loop(Vm *const vm, const uint32_t depth)
{
    const Frame *const f = &vm->frames[depth - 1];
    const ForLoop *const l = &f->loop;
    const Var *const v = symb_var(l->var);
    const Addr cell = v->vt;
    const bool int_var = is_int(v);
    uint32_t x;
    if (int_var) {
        int32_t sum;
        if (__builtin_add_overflow((int32_t)rd32(cell), (int32_t)l->step, &sum))
            repl_error(E_IQ);
        x = (uint32_t)sum;
    } else
        x = fmth_add(rd32(cell), l->step);
    wr32(cell, x);
    if (past_limit(l, int_var, x)) {
        vm->nframes = depth - 1;
        return;
    }
    vm->nframes = depth; /* the loops inside it end */
    go_to(vm, f->ret);
}

/* NEXT var. */
void flow_next_v(Vm *const vm)
{
    if (exec_stopchk(vm))
        return;
    drop_temps();
    const uint32_t depth = find_loop(vm, exec_u16(vm));
    if (!depth)
        repl_error(E_NF);
    step_loop(vm, depth);
}

/* NEXT: the innermost loop. */
void flow_next(Vm *const vm)
{
    if (exec_stopchk(vm))
        return;
    drop_temps();
    const uint32_t depth = vm->nframes;
    if (!depth || vm->frames[depth - 1].kind != FRAME_FOR)
        repl_error(E_NF);
    step_loop(vm, depth);
}

/* EXIT FOR. */
void flow_fpop(Vm *const vm)
{
    const Frame *const f = top_frame(vm);
    if (f && f->kind == FRAME_FOR)
        pop_to(vm, f);
}

/* ---- SUB and FUNCTION ----------------------------------------------------------------------- */

/* A scalar's value from cell from to cell to; a string's heap block follows its descriptor. */
static void move_value(const Var *const v, const Addr from, const Addr to)
{
    if (is_double(v))
        memcpy(at(to), at(from), 8);
    else if (is_string(v))
        flow_lsmove(from, to);
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
static void take_args(Vm *const vm, const Proc *const p, CallVar *const b)
{
    for (uint32_t i = p->npar; i-- > 0;) {
        b[i] = (CallVar){.var = blok_params[p->par + i]};
        const Var *const v = symb_var(b[i].var);
        const uint32_t ref = exec_pop(vm).u;
        if (!is_array(v) && ref != NO_REF) {
            b[i].cell = ref;
            vm->sp++; /* its value slot, empty */
            continue;
        }
        const Cell val = exec_pop(vm);
        if (is_array(v)) {
            b[i].arg = ref == NO_REF ? 0 : (uint16_t)ref;
            b[i].array.hdr = v->info == (VI_ARR | VK_INT) ? exec_record((uint16_t)val.u)
                                                          : symb_var((uint16_t)val.u)->vt;
        } else if (is_string(v))
            strg_set(val.a, addr_of(&b[i].value));
        else
            b[i].value = val;
    }
}

/* Set the procedure's locals aside into callvars from b, emptying them; returns past the last. */
static CallVar *save_locals(Vm *const vm, const uint8_t proc, CallVar *b)
{
    for (uint16_t id = symb.first_local[proc]; id; id = symb_varx(id)->nloc, b++) {
        if (b >= vm->callvars + MAX_CALLVARS - 1)
            repl_error(E_OS);
        *b = (CallVar){.var = id};
        Var *const v = symb_var(id);
        if (is_array(v)) {
            b->array.hdr = v->vt;
            v->vt = 0;
        } else {
            move_value(v, symb_cell(id), addr_of(&b->value));
            clear_value(v, symb_cell(id));
        }
    }
    return b;
}

/* Point the parameters at their arguments: a scalar by reference at the argument's cell, by value
   at its own cell given the value; an array at the argument's header. A scalar's cell before is
   kept in its CallVar. */
static void bind_params(CallVar *b, uint32_t n)
{
    for (; n--; b++) {
        Var *const v = symb_var(b->var);
        if (is_array(v)) {
            v->vt = b->array.hdr;
            if (b->arg) /* dynamic as the argument */
                symb_varx(b->var)->dyn = symb_varx(b->arg)->dyn;
            continue;
        }
        const Addr arg_cell = b->cell;
        b->cell = v->vt;
        if (arg_cell)
            v->vt = arg_cell;
        else {
            v->vt = symb_cell(b->var);
            move_value(v, addr_of(&b->value), v->vt);
        }
    }
}

/* PCALL proc body: call a SUB or FUNCTION, its arguments on the data stack. */
void flow_pcall(Vm *const vm)
{
    uint8_t *const call = vm->ip;
    const uint8_t proc = *call;
    const Proc *const p = &blok_procs[proc];
    if (vm->sp < vm->stklim || vm->nframes == MAX_FRAMES ||
        vm->ncallvars + p->npar >= MAX_CALLVARS - 1)
        repl_error(E_OS);
    CallVar *const params = vm->callvars + vm->ncallvars;
    take_args(vm, p, params);
    const CallVar *const end = save_locals(vm, proc, params + p->npar);
    bind_params(params, p->npar);

    vm->ip = call + 1 + sizeof(Jump); /* the call returns past its body's Jump */
    flow_push(vm, FRAME_CALL)->call = (Call){
        .proc = proc,
        .params = vm->ncallvars,
        .locals = vm->ncallvars + p->npar,
        .temp_floor = strg.tmp_floor,
        .stack = vm->sp,
    };
    vm->ncallvars = (uint32_t)(end - vm->callvars);
    strg.tmp_floor = strg.tmp_used;
    vm->ip = call + 1;
    take_jump(vm);
}

static bool is_param(const Proc *const p, const uint16_t var)
{
    for (uint32_t i = 0; i < p->npar; i++)
        if (blok_params[p->par + i] == var)
            return true;
    return false;
}

/* End call f, the innermost, and drop the frames above it. The array arguments get their headers
   back as the callee left them, a FUNCTION's result is kept, the locals get the caller's values
   back, the parameters their cells. */
static void end_call(Vm *const vm, const Frame *const f)
{
    const Call *const c = &f->call;
    const Proc *const p = &blok_procs[c->proc];
    CallVar *const params = vm->callvars + c->params, *const locals = vm->callvars + c->locals;
    pop_to(vm, f);

    for (CallVar *b = params; b < locals; b++)
        if (b->arg) {
            b->array.hdr = symb_var(b->var)->vt;
            b->array.dyn = symb_varx(b->var)->dyn;
        }
    if (p->kind == PK_FUNC) {
        const Var *const v = symb_var(p->result);
        const Addr cell = symb_cell(p->result);
        if (is_double(v))
            memcpy(&result.value, at(cell), 8);
        else if (is_string(v))
            flow_lsmove(cell, result.text);
        else
            result.value.u = rd32(cell);
    }
    for (CallVar *b = vm->callvars + vm->ncallvars; b-- > locals;) {
        Var *const v = symb_var(b->var);
        if (!is_array(v))
            move_value(v, addr_of(&b->value), symb_cell(b->var));
        else {
            if (v->vt && !is_param(p, b->var)) /* a parameter's array is the caller's */
                arry_free(b->var, 0);
            v->vt = b->array.hdr;
        }
    }
    for (CallVar *b = params; b < locals; b++) {
        if (!is_array(symb_var(b->var)))
            symb_var(b->var)->vt = b->cell;
        else if (b->arg) {
            symb_var(b->arg)->vt = b->array.hdr;
            symb_varx(b->arg)->dyn = b->array.dyn;
        }
    }
    vm->ncallvars = c->params;
    drop_temps(); /* the callee's */
    strg.tmp_floor = c->temp_floor;
}

/* PRET: return from the call, a FUNCTION's result pushed. */
void flow_pret(Vm *const vm)
{
    if (exec_stopchk(vm))
        return;
    const Frame *const f = flow_find(vm, FRAME_CALL);
    if (!f)
        repl_error(E_RG);
    end_call(vm, f);
    const Proc *const p = &blok_procs[f->call.proc];
    if (p->kind == PK_FUNC) {
        const Var *const v = symb_var(p->result);
        if (is_double(v))
            exec_push(vm, result.value);
        else if (is_string(v)) {
            if (strg.tmp_used >= DSZ * NTMP)
                repl_error(E_ST);
            const Addr t = strg.tmp + strg.tmp_used;
            strg.tmp_used += DSZ;
            flow_lsmove(result.text, t);
            exec_pusha(vm, t);
        } else
            exec_pusha(vm, result.value.u);
    }
    go_to(vm, f->ret);
}

void flow_lsmove(const Addr from, const Addr to)
{
    const Desc d = *(Desc *)at(from);
    *(Desc *)at(to) = d;
    if (d.len && d.ptr >= strg.low && d.ptr < allo.top)
        ((StrTail *)at(d.ptr + d.len))->owner = to;
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
void flow_onerr(Vm *const vm)
{
    vm->err.handler = handler_line(vm);
    if (!vm->err.handler && vm->err.active) {
        vm->err.user = vm->err.code;
        repl_error(E_USR);
    }
}

/* ON LOCAL ERROR GOTO: the running procedure's handler; in the module, ON ERROR's. */
void flow_onlerr(Vm *const vm)
{
    Frame *const f = flow_find(vm, FRAME_CALL);
    if (f)
        f->call.on_error = handler_line(vm);
    else
        flow_onerr(vm);
}

uint32_t flow_handler(Vm *const vm)
{
    Frame *h = nullptr;
    for (uint32_t i = vm->nframes; i-- > 0 && !h;)
        if (vm->frames[i].kind == FRAME_CALL && vm->frames[i].call.on_error)
            h = &vm->frames[i];
    if (!h)
        return vm->err.handler;
    for (const Frame *f; (f = flow_find(vm, FRAME_CALL)) != h;) {
        end_call(vm, f);
        go_to(vm, f->ret); /* the error is now at the call */
        vm->opip = --vm->ip;
    }
    return h->call.on_error;
}

/* The data stack as the running procedure's body started, or the module's. */
static Cell *stack_base(Vm *const vm)
{
    const Frame *const f = flow_find(vm, FRAME_CALL);
    return f ? f->call.stack : vm->rsp;
}

void flow_trap(Vm *const vm, const uint8_t code, const uint32_t handler)
{
    vm->err.code = code == E_USR ? vm->err.user : emsg_errnum[code];
    vm->err.active = true;
    vm->err.line = flow_erl(vm->line);
    uint32_t left;
    const Stmt *const s = flow_stmtof(vm->line, vm->opip, &left);
    vm->err.failed = (Place){vm->line, s->code};
    vm->err.next = (Place){vm->line, left > 1 ? s[1].code : LINE_END};
    chnl_clear();
    vm->sp = stack_base(vm);
    flow_jump(vm, handler);
    if (vm->running)
        longjmp(vm->loop, 1);
    exec_run();
}

uint16_t flow_erl(const uint32_t line)
{
    uint16_t erl = 0;
    for (uint32_t l = PROGBASE; l <= line && prog_line(l)->len; l += prog_line(l)->len)
        if (prog_line(l)->num != NONUM)
            erl = prog_line(l)->num;
    return erl;
}

Stmt *flow_stmtof(const uint32_t line, const uint8_t *const code, uint32_t *const left)
{
    uint8_t *const base = cache_at(prog_line(line)->code);
    const uint32_t off = (uint32_t)(code - base);
    Stmt *s = stmt_map(base, left);
    for (; *left > 1 && s[1].code <= off; s++)
        (*left)--;
    return s;
}

/* RESUME, RESUME NEXT, RESUME line. */
void flow_resume(Vm *const vm)
{
    if (exec_stopchk(vm))
        return;
    if (!vm->err.active) {
        vm->err.user = RESUME_WITHOUT_ERROR;
        repl_error(E_USR);
    }
    vm->err.active = false;
    vm->err.code = 0;
    vm->sp = stack_base(vm);
    const enum : uint8_t { AGAIN, NEXT, LINE } mode = exec_u8(vm);
    if (mode == LINE) {
        flow_jump(vm, flow_target(vm));
        return;
    }
    const Place p = mode == NEXT ? vm->err.next : vm->err.failed;
    drop_temps();
    vm->line = p.line;
    if (p.code == LINE_END)
        next_line(vm);
    else
        vm->ip = resolve(p.line, p.code);
}

/* A compile error, raised when its statement runs. */
void flow_err(Vm *const vm) { repl_error(((const ErrOp *)vm->opip)->code); }

/* ---- the rest ------------------------------------------------------------------------------- */

uint32_t flow_opsize(const uint8_t *const op)
{
    if (op_sizes[op[0]])
        return op_sizes[op[0]];
    if (op[0] == O_ON)
        return (op[1] & ON_COUNT) * sizeof(LineRef) + 2;
    return op[1] + 2u; /* its length byte */
}

void flow_setcont(Vm *const vm)
{
    if (vm->line != DLINE)
        vm->cont = flow_here(vm);
}

void flow_end(Vm *const vm)
{
    if (dbug.probing)
        dbug_done();
    chnl_closeall();
    vm->sp = vm->rsp;
    vm->nframes = 0;
    repl_ready();
}

void flow_nodir(Vm *const vm)
{
    if (vm->line == DLINE)
        repl_error(E_ID);
}
