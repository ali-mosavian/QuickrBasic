/* SKETCH, not built: flow.c as it would be written in C from the start, not translated from asm.
   Control flow of the VM: jumps, GOSUB, FOR/NEXT, SUB/FUNCTION calls, ON ERROR/RESUME. */
#include "vm/vm.h"

/* ---- vm.h (excerpt) ------------------------------------------------------------------------ */

/* A point in the program: a line and an offset into its compiled code. */
typedef struct {
    uint32_t line;
    uint32_t pc;
} Place;

typedef enum { FRAME_GOSUB, FRAME_FOR, FRAME_CALL, FRAME_EVENT } FrameKind;

typedef struct {
    uint16_t var;
    Value limit, step;
} ForLoop;

/* A local or parameter whose own storage is set aside while a call runs. */
typedef struct {
    uint16_t var;
    Value saved;    /* the caller's value, or its array */
    Value *storage; /* the parameter's own storage, while it aliases the argument */
} Binding;

typedef struct {
    const Proc *proc;
    Binding *bindings; /* on vm->bindings */
    uint32_t nbindings;
    Value *stack;      /* data stack at entry, for RESUME and errors */
    uint32_t on_error; /* ON LOCAL ERROR's handler line, 0 for none */
} Call;

typedef struct {
    FrameKind kind;
    Place ret; /* where RETURN, NEXT or the call's end goes */
    union {
        ForLoop loop;
        Call call;
        EventFrame event;
    };
} Frame;

typedef struct {
    Place at;          /* running statement */
    Value *sp;         /* data stack */
    Frame frames[MAX_FRAMES];
    uint32_t nframes;
    struct {
        uint32_t handler; /* ON ERROR GOTO line, 0 for none */
        bool active;      /* inside the handler */
        uint16_t code;
        Place failed, after; /* RESUME and RESUME NEXT */
    } err;
} Vm;

/* ---- the control stack ---------------------------------------------------------------------- */

static Frame *push_frame(Vm *vm, FrameKind kind)
{
    if (vm->nframes == MAX_FRAMES)
        vm_raise(vm, ERR_OUT_OF_STACK);
    Frame *f = &vm->frames[vm->nframes++];
    *f = (Frame){.kind = kind, .ret = vm->at};
    return f;
}

static Frame *top_frame(Vm *vm) { return vm->nframes ? &vm->frames[vm->nframes - 1] : nullptr; }

/* The innermost frame of kind, searching down to the first frame of any kind in stop. */
static Frame *find_frame(Vm *vm, FrameKind kind, uint32_t stop_kinds)
{
    for (uint32_t i = vm->nframes; i-- > 0;) {
        Frame *f = &vm->frames[i];
        if (f->kind == kind)
            return f;
        if (stop_kinds & 1u << f->kind)
            return nullptr;
    }
    return nullptr;
}

static void pop_to(Vm *vm, const Frame *f) { vm->nframes = (uint32_t)(f - vm->frames); }

/* ---- jumps ---------------------------------------------------------------------------------- */

void flow_goto(Vm *vm, uint32_t line)
{
    strings_drop_temps();
    vm->at = (Place){line, 0};
}

void flow_gosub(Vm *vm, uint32_t line)
{
    push_frame(vm, FRAME_GOSUB);
    flow_goto(vm, line);
}

void flow_return(Vm *vm)
{
    Frame *f = find_frame(vm, FRAME_GOSUB, 1u << FRAME_CALL);
    if (!f)
        vm_raise(vm, ERR_RETURN_WITHOUT_GOSUB);
    pop_to(vm, f); /* FOR loops inside the subroutine end too */
    vm->at = f->ret;
}

/* ON k GOTO/GOSUB: k counts from 1; out of range falls through. */
void flow_on(Vm *vm, int16_t k, const uint32_t *lines, uint32_t n, bool gosub)
{
    if (k < 1 || (uint32_t)k > n)
        return;
    if (gosub)
        flow_gosub(vm, lines[k - 1]);
    else
        flow_goto(vm, lines[k - 1]);
}

/* ---- FOR / NEXT ----------------------------------------------------------------------------- */

static bool loop_done(Value x, const ForLoop *l)
{
    int c = value_cmp(x, l->limit);
    return value_sign(l->step) < 0 ? c < 0 : c > 0;
}

/* FOR var = start TO limit STEP step; start is already stored. A loop not entered goes past its
   NEXT, at skip. */
void flow_for(Vm *vm, uint16_t var, Value limit, Value step, Place skip)
{
    Frame *old = find_frame(vm, FRAME_FOR, ~(1u << FRAME_FOR));
    if (old && old->loop.var == var)
        pop_to(vm, old); /* reentering a loop restarts it */
    ForLoop loop = {var, limit, step};
    if (loop_done(*var_value(var), &loop)) {
        vm->at = skip;
        return;
    }
    push_frame(vm, FRAME_FOR)->loop = loop;
}

/* NEXT [var]: step the loop; back to its body until done. */
void flow_next(Vm *vm, uint16_t var)
{
    Frame *f = top_frame(vm);
    if (var && f && f->kind == FRAME_FOR && f->loop.var != var)
        f = find_frame(vm, FRAME_FOR, 1u << FRAME_GOSUB | 1u << FRAME_CALL);
    if (!f || f->kind != FRAME_FOR || (var && f->loop.var != var))
        vm_raise(vm, ERR_NEXT_WITHOUT_FOR);

    Value *x = var_value(f->loop.var);
    *x = value_add(*x, f->loop.step); /* raises on INT overflow */
    if (loop_done(*x, &f->loop))
        pop_to(vm, f);
    else
        vm->at = f->ret;
}

/* ---- SUB / FUNCTION ------------------------------------------------------------------------- */

/* Each argument is a variable passed by reference, or a value. */
typedef struct {
    Value *ref; /* nullptr: by value */
    Value value;
} Arg;

static void bind_param(Binding *b, uint16_t param, const Arg *arg)
{
    *b = (Binding){.var = param, .saved = *var_value(param), .storage = var_storage(param)};
    if (arg->ref)
        var_alias(param, arg->ref);
    else
        *var_value(param) = value_copy(arg->value);
}

static void save_local(Binding *b, uint16_t local)
{
    *b = (Binding){.var = local, .saved = *var_value(local)};
    *var_value(local) = value_empty(var_type(local));
}

void flow_call(Vm *vm, const Proc *p, const Arg *args)
{
    Frame *f = push_frame(vm, FRAME_CALL);
    Binding *b = bindings_alloc(p->nparams + p->nlocals);
    for (uint32_t i = 0; i < p->nparams; i++)
        bind_param(&b[i], p->params[i], &args[i]);
    for (uint32_t i = 0; i < p->nlocals; i++)
        save_local(&b[p->nparams + i], p->locals[i]);
    f->call = (Call){p, b, p->nparams + p->nlocals, vm->sp, 0};
    vm->at = p->body;
}

/* Undo a call's bindings, newest first; a FUNCTION's result is returned. */
static Value end_call(Vm *vm, Frame *f)
{
    const Call *c = &f->call;
    Value result = c->proc->is_function ? value_take(var_value(c->proc->result)) : (Value){};
    for (uint32_t i = c->nbindings; i-- > 0;) {
        Binding *b = &c->bindings[i];
        if (b->storage)
            var_unalias(b->var, b->storage);
        else
            value_free(var_value(b->var));
        *var_value(b->var) = b->saved;
    }
    bindings_free(c->bindings);
    strings_drop_temps();
    pop_to(vm, f);
    vm->at = f->ret;
    return result;
}

void flow_return_from_call(Vm *vm)
{
    Frame *f = find_frame(vm, FRAME_CALL, 0);
    if (!f)
        vm_raise(vm, ERR_RETURN_WITHOUT_GOSUB);
    Value result = end_call(vm, f);
    if (f->call.proc->is_function)
        vm_push(vm, result);
}

/* ---- ON ERROR / RESUME ---------------------------------------------------------------------- */

/* The handler for an error here: the innermost call's ON LOCAL ERROR, else ON ERROR. Calls above
   the handler's end, as if the error happened at their call site. */
static uint32_t find_handler(Vm *vm)
{
    for (uint32_t i = vm->nframes; i-- > 0;) {
        Frame *f = &vm->frames[i];
        if (f->kind != FRAME_CALL || !f->call.on_error)
            continue;
        while (top_frame(vm) != f)
            end_call(vm, find_frame(vm, FRAME_CALL, 0));
        return f->call.on_error;
    }
    return vm->err.handler;
}

/* Called from vm_raise when a handler is set; never returns. */
[[noreturn]] void flow_trap(Vm *vm, uint16_t code)
{
    uint32_t handler = find_handler(vm);
    vm->err.code = code;
    vm->err.active = true;
    vm->err.failed = vm->at;
    vm->err.after = next_statement(vm->at);
    Frame *call = find_frame(vm, FRAME_CALL, 0);
    vm->sp = call ? call->call.stack : vm->stack_base;
    files_reset_channel();
    flow_goto(vm, handler);
    vm_continue(vm);
}

typedef enum { RESUME_SAME, RESUME_NEXT, RESUME_LINE } ResumeKind;

void flow_resume(Vm *vm, ResumeKind kind, uint32_t line)
{
    if (!vm->err.active)
        vm_raise(vm, ERR_RESUME_WITHOUT_ERROR);
    vm->err.active = false;
    switch (kind) {
    case RESUME_SAME: vm->at = vm->err.failed; break;
    case RESUME_NEXT: vm->at = vm->err.after; break;
    case RESUME_LINE: flow_goto(vm, line); break;
    }
}
