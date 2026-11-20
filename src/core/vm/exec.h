/* The executor's interface. How it works: exec.c.
 *
 * The VM's state, the data stack, the ops' records and numbers, and how code is encoded.
 *
 * The code is a stream of byte ops in the code cache, each its op then its operands. The loop reads
 * the op at vm.ip, keeps its place in vm.opip and calls its handler with vm.ip at its operands; the
 * handler moves vm.ip past them, or to a jump's target, and returns. The data stack is 8-byte Cells
 * in the arena, growing down from its top: vm.sp the top cell. Every op takes its operands off it
 * and pushes its result; at a statement's start sp is vm.rsp.
 *
 * Errors unwind with longjmp: repl_error (io/repl) to READY or to the compiler; an error ON ERROR
 * handles goes back into the loop at the handler (flow_trap). A run ends at READY (repl_ready,
 * io/repl), never by returning from exec_run.
 *
 * # asm structure (retired/asm32/vm/vm.inc, state.inc)
 * | asm | here | |
 * |---|---|---|
 * | TJUMP, JSIZE | Jump, JSIZE | MIRROR |
 * | TERR | ErrOp | MIRROR |
 * | TLREF | LineRef | MIRROR |
 * | CELL, dpush, dpop | Cell; exec_push, exec_pushi/a/d, exec_pop, exec_popi/a/d | MIRROR |
 * | dispatch | exec_run's loop (exec.c) | MOVED |
 * | vslot, bvar | exec_cell | D4 |
 * | ipload | flow_place | MOVED |
 * | subchk | subscript (exec.c) | MOVED |
 * | tspclr | strg.tmp_used = strg.tmp_floor | MOVED |
 * | stopchk | exec_stopchk (flow.c) | MOVED |
 * | a1get | inside exec_*_a1 (exec.c) | MOVED |
 * | TRUN (g_state.rn) | Vm | MIRROR |
 * | opip, rsp, stklim, cstk, lsp | vm.opip, vm.rsp, vm.stklim, vm.frames, vm.fp | D1 |
 * | cplim | comp.depth (comp's) | MOVED |
 * | line, runln, lnum | vm.line, vm.runln, vm.lnum | MIRROR |
 * | stpflg | vm.stop (volatile) | MIRROR |
 * | contln, contcode | vm.cont.line, vm.cont.code | MIRROR |
 * | onerr, inerr, errn, erl, uerr | vm.err.handler, active, code, line, user | MIRROR |
 * | resln, resat, resnext | vm.err.failed, vm.err.next | D2 |
 * | lfrom, lto | list_range's parameters (comp) | MOVED |
 * | narr | arry_*'s n parameter (mem) | MOVED |
 * | stv, aret, cnarg, cop, fid, flim, fstep, fop, fvp, fsv, isw_si, lk, onk, runl, ppip, ppend, pres | locals, statics | MIRROR |
 * | dlp, dataat, dyo, inend, iny0, iredo | StmtState (stmts.h) | MOVED |
 * | (none) | vm.ip, vm.sp, vm.stak, vm.loop, vm.running | D3 |
 *
 * - D1. The control stack is vm.frames, typed records; no save stack: a call's locals and
 *   arguments are its frame on the data stack (vm/flow.c).
 * - D2. RESUME's places are Places in vm.err.
 * - D3. ESI and ESP are vm.ip and vm.sp; a handler returns where the asm jumps to dispatch. vm.loop
 *   is the longjmp target for flow_trap.
 * - D4. A variable op holds its cell (VCELL), set at link, not the variable's id.
 */
#ifndef CORE_VM_EXEC_H
#define CORE_VM_EXEC_H
#include "defs.h"
#include "fault.h"
#include <setjmp.h>
typedef struct Arry Arry;
typedef struct ArrView ArrView;
typedef struct Symb Symb;

/* A jump op's target. */
typedef struct PACKED {
    uint32_t line;
    uint32_t body; /* the statement's body offset */
    uint32_t dest; /* its code's offset in the cache; under 3 the mode, till found (flow) */
} Jump;
enum { JSIZE = 1 + sizeof(Jump) };
/* PCALL's operands: the procedure, its frame's cells and its parameters (link's), its body's Jump. */
enum {
    PCALL_PROC = 1,
    PCALL_CELLS = 5,
    PCALL_NPAR = 9,
    PCALL_JUMP = 11,
    PCALL_SIZE = PCALL_JUMP + sizeof(Jump)
};
/* An FV op's operand: the variable's id as compiled, its cell's Addr once linked. */
enum { VCELL = 4 };
/* An FA or FP op's operand: the variable's id; an FP op's its ref cell's frame offset once linked. */
enum { VID = 4 };
/* I.NEXT.UP, I.NEXT.DN: counter, step, limit cells, the Jump back */
enum { NEXT_SIZE = 1 + 3 * VCELL + sizeof(Jump) };
/* EOL.TO: the next line, its code's offset in the cache */
enum { EOL_TO_SIZE = 1 + 2 * sizeof(uint32_t) };

/* An ERR op: a compile error met running. */
typedef struct PACKED {
    uint8_t op;
    uint8_t code;   /* the error, E_* */
    uint8_t dg;     /* its DG_ (io/msg), 0: by its text */
    uint32_t at;    /* the body's byte it is at */
    uint16_t state; /* the automaton's state, FFFFh none */
} ErrOp;

/* A GOTO's line: its number (NONUM a label's) and the line, link_resolve's. Until then a line the
   compiler did not find is LREF_AT and the label's body offset, where its error points. */
typedef struct PACKED {
    uint16_t num;
    uint32_t line;
} LineRef;
enum : uint32_t { LREF_AT = 0x80000000 };

/* vm.stop's bits past defs.h's SF_BRK, SF_EV: a tick or a trapped key to count (evnt_poll). */
enum : uint8_t { SF_TICK = 4 };

/* An op's flags (ops.def). */
enum : uint8_t { FX = 1, FV = 2, FB = 4, FK = 8, FV2 = 16, FV3 = 32, FP = 64, FA = 128 };

enum : uint8_t {
#define OP(name, h, size, fl) O_##name,
#include "vm/ops.def"
    O_NOPS
};

enum : uint8_t {
#define K(name, h) K_##name,
#include "vm/ops.def"
    K_N,
    K_FN = K_FN_SGN, /* + a function's token - TK_SGN */
};
static_assert(K_IPOW == 40 && K_FN == 45 && K_LEFT == 65 && K_MID2 == 68 && K_FIX == 69);

/* A place in the program: a line (an offset in the program store, DLINE the direct line) and an
   offset of code from the cache's base. */
typedef struct {
    uint32_t line;
    uint32_t code;
} Place;

/* CALL_X: a call whose return does more, off exec_run's inline PRET: its own segment (vm/stack.c),
   or its locals' arrays on the heap (mem/array.c). NONE: the floor below the first frame. */
/* FRAME_MODULE: a module's code an IMPORT runs (vm/flow.c flow_import). */
typedef enum : uint8_t {
    FRAME_GOSUB,
    FRAME_EVENT,
    FRAME_CALL,
    FRAME_CALL_X,
    FRAME_MODULE,
    FRAME_NONE
} FrameKind;

/* A SUB or FUNCTION running. */
typedef struct {
    uint32_t proc;
    uint32_t temp_floor; /* the caller's floor of string temporaries */
    Cell *stack;         /* the data stack at its body's start, past its locals */
    Addr fp;             /* its frame (vm.fp): npar arguments above it */
    uint16_t npar;
    Addr caller;       /* the caller's */
    uint32_t on_error; /* ON LOCAL ERROR's line, 0 none */
    Cell *back;        /* CALL_X's: in a segment of its own, the caller's sp past the arguments */
    uint32_t seg;      /*   the caller's segment */
    bool heap;         /* CALL_X's: its locals' arrays on the heap, freed at its return */
} Call;

typedef struct {
    FrameKind kind;
    Place ret; /* where RETURN or the call's end goes */
    union {
        Call call;
        uint8_t event; /* the event's slot */
    };
} Frame;

static inline bool frame_is_call(const Frame *const f)
{
    return f->kind == FRAME_CALL || f->kind == FRAME_CALL_X;
}

/* A segment of the data stack: [base, end). */
typedef struct {
    Addr base, end;
} StakSeg;

/* The data stack's segments, each below the one before (vm/stack.c); sp in the cur'th. */
typedef struct {
    StakSeg *seg;
    uint32_t n, cap, cur;
} Stak;

/* The run's state. */
typedef struct Vm {
    uint8_t *ip;   /* the running op's operands, the next op */
    uint8_t *opip; /* the op dispatch ran last: CONT after BREAK, an error's address */
    Cell *sp;      /* the data stack's top cell */
    Cell *rsp;     /* the stack empty, at a statement's start */
    Cell *stklim;  /* PCALL's floor of sp: its segment's base past STKEVAL */
    Stak stak;     /* the data stack's segments (vm/stack.c) */
    Addr fp;       /* the running procedure's frame: its link cell, locals below, arguments above */

    Frame *frames;   /* the first; frames[-1] the floor, FRAME_NONE (vm/stack.c) */
    Frame *ftop;     /* past the last */
    Frame *fend;     /* past the room, grown as calls nest */
    uint32_t fbytes; /* the room's bytes from the floor */

    uint32_t line;         /* the running line */
    uint16_t lnum;         /* a line number to find, as prog_find's */
    volatile uint8_t stop; /* SF_*; the platform's interrupts set SF_BRK, SF_TICK */

    Place cont; /* where CONT goes on; line 0 none, code 0 CONT ends again */

    struct {
        uint32_t handler; /* ON ERROR's line, 0 none */
        bool active;      /* in the handler, till RESUME */
        uint16_t code;    /* ERR */
        uint16_t line;    /* ERL */
        uint16_t user;    /* ERROR n's n, raised as E_USR */
        Place failed;     /* RESUME's statement */
        Place next;       /* RESUME NEXT's; code LINE_END the next line */
    } err;

#ifdef EXEC_COUNT

    uint64_t count; /* the ops dispatched: the host build's instruction count */
#endif
    bool running; /* exec_run's loop is on the C stack: vm.loop holds */
    jmp_buf loop; /* exec_run's: flow_trap comes back into the loop */
} Vm;

static inline uint32_t vm_nframes(const Vm *const v) { return (uint32_t)(v->ftop - v->frames); }

static inline void exec_counted(Vm *const vm)
{
#ifdef EXEC_COUNT
    vm->count++;
#else
    (void)vm;
#endif
}

/* The data stack. */
static inline void exec_push(Vm *const vm, Cell c) { *--vm->sp = c; }
/* An INT or an Addr fills only the cell's low dword. */
static inline void exec_pushi(Vm *const vm, int32_t v) { (--vm->sp)->i = v; }
static inline void exec_pusha(Vm *const vm, Addr a) { (--vm->sp)->a = a; }
static inline void exec_pushd(Vm *const vm, double d) { *--vm->sp = (Cell){.d = d}; }
static inline Cell exec_pop(Vm *const vm) { return *vm->sp++; }
static inline int32_t exec_popi(Vm *const vm) { return vm->sp++->i; }
static inline Addr exec_popa(Vm *const vm) { return vm->sp++->a; }
static inline double exec_popd(Vm *const vm) { return vm->sp++->d; }

/* The operands at vm->ip, vm->ip past them. */
static inline uint8_t exec_u8(Vm *const vm) { return *vm->ip++; }
static inline uint16_t exec_u16(Vm *const vm)
{
    const uint16_t v = load16(vm->ip);
    vm->ip += 2;
    return v;
}
static inline uint32_t exec_u32(Vm *const vm)
{
    const uint32_t v = load32(vm->ip);
    vm->ip += 4;
    return v;
}
/* An FV op's cell. */
static inline Addr exec_cell(Vm *const vm) { return exec_u32(vm); }

/* The data stack carved from the arena (allo_carve); the control stack is
   mem's, allo.cs_lim .. allo.cs_top. The tick's and the trapped keys' flag pointed at vm.stop
   (SF_TICK); psys_exit takes the tick out. */
void exec_init(Session *S);
/* ex_lp: run ops from vm.ip, from here on; leaves only by longjmp. */
[[noreturn]]
void exec_run(Session *S);
/* Run ops from vm.ip until it reaches end. */
void exec_until(Session *S, const uint8_t *end);
/* Op op's flags: FX, FV, FB, FK. */
uint8_t exec_flags(uint8_t op);
/* ex_brk: the STOP check found vm.stop or the platform's flags set. true: the op is not to run,
   vm.ip moved (an event's GOSUB, or the IDE's step back at the op); false: go on. BREAK raises. */
bool exec_brk(Session *S);
/* arr_r1: element sub, size bytes each, of the 1-D array the X.A1 op at vm.ip names; frame: an
   X.A1F's, the array in the frame. Subscript out of range past a word. */
Addr exec_elem1(Session *S, int32_t sub, uint32_t size, bool frame);
/* arr_pn: the element an FA op's `id:4 cell:4 n:1` at vm.ip names, its n subscripts popped. */
Addr exec_elem(Arry *ar, Vm *vm);
/* An array operand popped (defs.h ARR_FIELD), ours: its cell, *v its array's when it has one; null
   a fixed array field's, *v its. */
ArrSlot *exec_arrayop(Arry *ar, Vm *vm, ArrView *v);
/* rec_get: record id's header (s its cell), the record made, zeroed, at its first use. */
Addr exec_record(Arry *ar, ArrSlot *s, uint32_t id);
/* int_div: a / b truncated, *rem its remainder. Division by zero; Overflow -2^31 / -1. */
int32_t exec_idiv(Fault *f, int32_t a, int32_t b, int32_t *rem);
#endif
