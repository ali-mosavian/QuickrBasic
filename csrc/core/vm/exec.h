/* The executor (src/vm/exec.asm): the run's state, the dispatch loop, the data stack, the ops'
   records and numbers (vm/ops.def).

   The code is a stream of byte ops in the code cache, each its op then its operands. The loop
   reads the op at exec.ip, keeps its place in exec.opip and calls its handler with exec.ip at its
   operands; the handler moves exec.ip past them, or to a jump's target, and returns. The data
   stack is 8-byte Cells in the arena, growing down from its top: exec.sp the top cell. Every op
   takes its operands off it and pushes its result; at a statement's start sp is exec.rsp.

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

/* exec.stop's bits past defs.h's SF_BRK, SF_EV: a tick or a trapped key to count (evnt_poll). */
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

/* The run's state (state.inc's TRUN). Lines are offsets in the program store (mem/prog), DLINE
   the direct line. A code offset kept in memory is from the cache's base: under DCODE an offset in
   its line's code (cm_flush), resolved by flow_resolve. */
typedef struct {
    uint8_t *ip;   /* ESI: the running op's operands, the next op */
    uint8_t *opip; /* rn.opip: the op dispatch ran last: CONT after BREAK, an error's address */
    Cell *sp;      /* ESP: the data stack's top cell */
    Cell *rsp;     /* rn.rsp: the stack empty, at a statement's start */
    Cell *stklim;  /* rn.stklim: PCALL's floor of sp */
    Cell *stktop;  /* the data stack's region's top (STKTOP); its base stktop - STKSZ/8 */
    uint32_t cplim; /* rn.cplim: the compiler's frames' floor (comp's) */

    Addr cstk;  /* rn.cstk: the control stack's top frame; allo.cs_top (CSTOP) when empty */
    Addr lsav;  /* g_lsav: the save stack, LSAVSZ bytes of LSSZ entries */
    Addr lsp;   /* rn.lsp: its top */

    uint32_t line;  /* rn.line: the running line */
    uint32_t runln; /* rn.runln: the line flow_line runs */
    uint16_t lnum;  /* rn.lnum: a line number to find, as prog_find's */
    volatile uint8_t stop; /* rn.stpflg: SF_*; the platform's interrupts set SF_BRK, SF_TICK */

    uint32_t cont_line; /* rn.contln: where CONT goes on, 0 none */
    uint32_t cont_code; /* rn.contcode: its code, an offset; 0 CONT ends again */

    uint32_t onerr;    /* rn.onerr: ON ERROR's line, 0 none */
    bool inerr;        /* rn.inerr: in the handler, till RESUME */
    uint16_t err;      /* rn.errn: ERR */
    uint16_t erl;      /* rn.erl: ERL */
    uint16_t uerr;     /* rn.uerr: ERROR n's n, raised as E_USR */
    uint32_t res_line; /* rn.resln: RESUME's line */
    uint32_t res_at;   /* rn.resat: its failing statement's code offset */
    uint32_t res_next; /* rn.resnext: the next's; FFFFFFFFh the next line */

    uint16_t list_from, list_to; /* rn.lfrom, rn.lto: LIST's range (comp/list) */

    jmp_buf loop; /* exec_run's: flow_trap and a nested probe come back into the loop */
} Exec;
extern Exec exec;

/* The data stack. */
static inline void exec_push(Cell c) { *--exec.sp = c; }
static inline void exec_pushi(int32_t v) { *--exec.sp = (Cell){.raw = (uint32_t)v}; }
static inline void exec_pusha(Addr a) { *--exec.sp = (Cell){.raw = a}; }
static inline void exec_pushd(double d) { *--exec.sp = (Cell){.d = d}; }
static inline Cell exec_pop(void) { return *exec.sp++; }
static inline int32_t exec_popi(void) { return exec.sp++->i; }
static inline Addr exec_popa(void) { return exec.sp++->a; }
static inline double exec_popd(void) { return exec.sp++->d; }

/* The operands at exec.ip, exec.ip past them. */
static inline uint8_t exec_u8(void) { return *exec.ip++; }
static inline uint16_t exec_u16(void) { uint16_t v; memcpy(&v, exec.ip, 2); exec.ip += 2; return v; }
static inline uint32_t exec_u32(void) { uint32_t v; memcpy(&v, exec.ip, 4); exec.ip += 4; return v; }

/* The data stack and the save stack carved from the arena (allo_carve); the control stack is
   mem's, allo.cs_lim .. allo.cs_top. The platform's flags pointed at exec.stop. */
void exec_init(void);
/* ex_lp: run ops from exec.ip, from here on; leaves only by longjmp. */
[[noreturn]] void exec_run(void);
/* ex_brk: the STOP check found exec.stop or the platform's flags set. true: the op is not to run,
   exec.ip moved (an event's GOSUB, or the IDE's step back at the op); false: go on. BREAK raises. */
bool exec_brk(void);
/* stopchk: the STOP check at jumps, line ends, RETURN, NEXT; true: return from the handler. */
static inline bool exec_stopchk(void) { return exec.stop && exec_brk(); }
/* arr_r1: element sub of 1-D array id, its Addr. Subscript out of range past a word. */
Addr exec_elem1(uint16_t id, int32_t sub);
/* arr_pn: the element an op's `id:2 n:1` at exec.ip names, its n subscripts popped. */
Addr exec_elem(void);
/* rec_get: record id's header, the record made, zeroed, at its first use. */
Addr exec_record(uint16_t id);
/* int_div: a / b truncated, *rem its remainder. Division by zero; Overflow -2^31 / -1. */
int32_t exec_idiv(int32_t a, int32_t b, int32_t *rem);
#endif
