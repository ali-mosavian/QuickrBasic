/* The executor (src/vm/exec.asm): the run's state, the dispatch loop, the data stack, the ops'
   records and numbers (vm/ops.def).

   The code is a stream of byte ops in the code cache, each its op then its operands. The loop
   reads the op at vm.ip, keeps its place in vm.opip and calls its handler with vm.ip at its
   operands; the handler moves vm.ip past them, or to a jump's target, and returns. The data
   stack is 8-byte Cells in the arena, growing down from its top: vm.sp the top cell. Every op
   takes its operands off it and pushes its result; at a statement's start sp is vm.rsp.

   Errors unwind with longjmp: repl_error (io/repl) to READY or to the compiler; an error ON ERROR
   handles goes back into the loop at the handler (flow_trap). A run ends at READY (repl_ready,
   io/repl), never by returning from exec_run. */
#ifndef CORE_VM_EXEC_H
#define CORE_VM_EXEC_H
#include "defs.h"
#include <setjmp.h>

/* A jump op's target. */
typedef struct PACKED {
    uint32_t line;
    uint32_t body; /* the statement's body offset */
    uint32_t dest; /* its code's offset in the cache; under 3 the mode, till found (flow) */
} Jump;
enum { JSIZE = 1 + sizeof(Jump) };

/* An ERR op: a compile error met running. */
typedef struct PACKED {
    uint8_t op;
    uint8_t code;   /* the error, E_* */
    uint8_t dg;     /* its DG_ (io/msg), 0: by its text */
    uint32_t at;    /* the body's byte it is at */
    uint16_t state; /* the automaton's state, FFFFh none */
} ErrOp;

/* A GOTO's line: its number, the line (0 till the first jump finds it, flow_target). */
typedef struct PACKED {
    uint16_t num;
    uint32_t line;
} LineRef;

/* vm.stop's bits past defs.h's SF_BRK, SF_EV: a tick or a trapped key to count (evnt_poll). */
enum : uint8_t { SF_TICK = 4 };

/* An op's flags (ops.def). */
enum : uint8_t { FX = 1, FV = 2, FB = 4 };

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
static_assert(K_IPOW == 42 && K_FN == 47 && K_LEFT == 67 && K_MID2 == 70 && K_FIX == 71);

/* A place in the program: a line (an offset in the program store, DLINE the direct line) and an
   offset of code from the cache's base. Under DCODE the offset is in the line's own code, which a
   flush of the cache keeps valid (flow_place). */
typedef struct {
    uint32_t line;
    uint32_t code;
} Place;

typedef enum : uint8_t { FRAME_GOSUB, FRAME_EVENT, FRAME_FOR, FRAME_CALL } FrameKind;

/* A FOR loop's variable, limit and step: INTs, or SINGLEs' bits. */
typedef struct {
    uint16_t var;
    uint32_t limit, step;
} ForLoop;

/* A SUB or FUNCTION running. Its parameters' CallVars start at params, its locals' at locals; they
   end at vm.ncallvars once the frames above it are gone. */
typedef struct {
    uint8_t proc;
    uint32_t params, locals;
    uint16_t temp_floor; /* the caller's floor of string temporaries */
    Cell *stack;         /* the data stack at its body's start */
    uint32_t on_error;   /* ON LOCAL ERROR's line, 0 none */
} Call;

typedef struct {
    FrameKind kind;
    Place ret; /* where RETURN, the call's end or NEXT goes: a FOR's is its body */
    union {
        ForLoop loop;
        Call call;
        uint8_t event; /* the event's slot */
    };
} Frame;

enum { MAX_FRAMES = 800 };

/* A procedure's parameter or local, set aside while its call runs. A local's value (an array's
   header) is the caller's; an array parameter's header is the argument's, handed back at the end;
   a scalar parameter's cell is its cell before the call. */
typedef struct PACKED {
    uint16_t var;
    uint16_t arg; /* an array parameter's argument, 0 none */
    union {
        Cell value;
        struct PACKED {
            Addr hdr;
            uint8_t dyn; /* REDIM made it */
        } array;
    };
    Addr cell; /* a scalar parameter's; 0 for a local */
} CallVar;
static_assert(sizeof(CallVar) == 16);

enum { MAX_CALLVARS = LSAVSZ / sizeof(CallVar) };

/* The run's state. */
typedef struct Vm {
    uint8_t *ip;   /* the running op's operands, the next op */
    uint8_t *opip; /* the op dispatch ran last: CONT after BREAK, an error's address */
    Cell *sp;      /* the data stack's top cell */
    Cell *rsp;     /* the stack empty, at a statement's start */
    Cell *stklim;  /* PCALL's floor of sp */
    Cell *stktop;  /* the data stack's region's top; its base stktop - STKSZ/8 */

    Frame frames[MAX_FRAMES];
    uint32_t nframes;
    CallVar *callvars; /* in the arena: a string set aside names its CallVar as its block's owner */
    uint32_t ncallvars;

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

    bool running; /* exec_run's loop is on the C stack: vm.loop holds */
    jmp_buf loop; /* exec_run's: flow_trap comes back into the loop */
} Vm;
extern Vm vm;

/* The data stack. */
static inline void exec_push(Vm *vm, Cell c) { *--vm->sp = c; }
/* An INT or an Addr fills only the cell's low dword. */
static inline void exec_pushi(Vm *vm, int32_t v) { (--vm->sp)->i = v; }
static inline void exec_pusha(Vm *vm, Addr a) { (--vm->sp)->a = a; }
static inline void exec_pushd(Vm *vm, double d) { *--vm->sp = (Cell){.d = d}; }
static inline Cell exec_pop(Vm *vm) { return *vm->sp++; }
static inline int32_t exec_popi(Vm *vm) { return vm->sp++->i; }
static inline Addr exec_popa(Vm *vm) { return vm->sp++->a; }
static inline double exec_popd(Vm *vm) { return vm->sp++->d; }

/* The operands at vm->ip, vm->ip past them. */
static inline uint8_t exec_u8(Vm *vm) { return *vm->ip++; }
static inline uint16_t exec_u16(Vm *vm)
{
    const uint16_t v = load16(vm->ip);
    vm->ip += 2;
    return v;
}
static inline uint32_t exec_u32(Vm *vm)
{
    const uint32_t v = load32(vm->ip);
    vm->ip += 4;
    return v;
}

/* The data stack and the save stack carved from the arena (allo_carve); the control stack is
   mem's, allo.cs_lim .. allo.cs_top. The tick's and the trapped keys' flag pointed at vm.stop
   (SF_TICK); psys_exit takes the tick out. */
void exec_init(void);
/* ex_lp: run ops from vm.ip, from here on; leaves only by longjmp. */
[[noreturn]] void exec_run(void);
/* ex_brk: the STOP check found vm.stop or the platform's flags set. true: the op is not to run,
   vm.ip moved (an event's GOSUB, or the IDE's step back at the op); false: go on. BREAK raises. */
bool exec_brk(Vm *vm);
/* stopchk: the STOP check at jumps, line ends, RETURN, NEXT; true: return from the handler. */
static inline bool exec_stopchk(Vm *vm) { return vm->stop && exec_brk(vm); }
/* arr_r1: element sub of 1-D array id, its Addr. Subscript out of range past a word. */
Addr exec_elem1(uint16_t id, int32_t sub);
/* arr_pn: the element an op's `id:2 n:1` at vm.ip names, its n subscripts popped. */
Addr exec_elem(Vm *vm);
/* rec_get: record id's header, the record made, zeroed, at its first use. */
Addr exec_record(uint16_t id);
/* int_div: a / b truncated, *rem its remainder. Division by zero; Overflow -2^31 / -1. */
int32_t exec_idiv(int32_t a, int32_t b, int32_t *rem);
#endif
