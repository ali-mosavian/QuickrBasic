/* Control flow's interface. How it works: flow.c. */
#ifndef CORE_VM_FLOW_H
#define CORE_VM_FLOW_H
#include "vm/exec.h"
#include "mem/program.h"
#include "mem/string.h"
#include "mem/symbols.h"

enum : uint32_t { LINE_END = 0xFFFFFFFF }; /* a Place's code: the start of the line after */

/* The FUNCTION result's string descriptor carved from the arena. */
void flow_init(Session *S);

/* Push a frame of kind, returning where the running statement goes on. Out of stack space. */
Frame *flow_push(Vm *vm, const Allo *al, Fault *f, FrameKind kind);
/* The innermost frame of kind, nullptr none. */
Frame *flow_find(Vm *vm, FrameKind kind);
/* The place of vm->ip in the running line. */
Place flow_here(const Vm *vm, const Allo *al);
/* The code at place, its line compiled if the cache dropped it. */
uint8_t *flow_place(const Allo *al, Place place);

/* Go to line's start, linking the program first if it is not. */
void flow_jump(Session *S, uint32_t line);
/* flow_nextln when the next line is a stored one: the temporaries dropped, vm->line that line,
   its code returned; nullptr, nothing changed: the program's end or the direct line. */
static inline uint8_t *flow_next_code(
    Vm *const vm,
    Strg *const sg,
    const Prog *const pg,
    const Allo *const al
)
{
    const uint32_t next = vm->line + prog_line(pg, vm->line)->len;
    const Line *const ln = prog_line(pg, next);

    if (!ln->len)
        return nullptr;

    sg->tmp_used = sg->tmp_floor;
    vm->line = next;
    uint8_t *const code = at(al->cache + ln->code);
    __builtin_assume(code != nullptr); /* as flow_jump_code's */
    return code;
}

/* The data stack at a statement's start: the innermost call's body's, else vm.rsp. */
Cell *flow_stack_base(Vm *vm);

/* The Jump at ip taken: the temporaries dropped, vm->line its line, its target's code returned;
   nullptr, nothing changed: not resolved (a direct line's block jump). */
static inline uint8_t *flow_jump_code(
    Vm *const vm,
    Strg *const sg,
    const Allo *const al,
    const uint8_t *const ip
)
{
    const Jump j = *(const Jump *)ip;

    if (j.dest <= TO_NEXT_STMT) /* still its JumpMode */
        return nullptr;

    sg->tmp_used = sg->tmp_floor;
    vm->line = j.line;
    uint8_t *const code = at(al->cache + j.dest);
    __builtin_assume(code != nullptr); /* the cache is past 0: a caller's null test is the mode's */
    return code;
}

/* Go to the next line; past the last the program ENDs. */
void flow_nextln(Session *S);
/* The LineRef operand at vm->ip: its line, found by number on first use. Undefined line number. */
uint32_t flow_target(Vm *vm);
/* The direct line compiled into the cache; returns its code. Out of memory. */
uint8_t *flow_dcode(Session *S);

/* The handler for an error now: the innermost call's ON LOCAL ERROR, the calls above it returned
   as if the error were at their call; else ON ERROR's. 0 none. */
uint32_t flow_handler(Session *S);
/* Run handler for error code: sets ERR, ERL and RESUME's places, then back into the loop. */
[[noreturn]]
void flow_trap(Session *S, uint8_t code, uint32_t handler);
/* ERL: the last line number at or before line, 0 none. */
uint16_t flow_erl(Prog *pg, uint32_t line);
/* The statement map entry of the statement holding code in line; *left the entries from it on. */
Stmt *flow_stmtof(
    const Prog *pg,
    const Allo *al,
    uint32_t line,
    const uint8_t *code,
    uint32_t *left
);

/* CONT goes on at the running statement; not from the direct line. */
void flow_setcont(Vm *vm, const Allo *al);
/* The program ends: files closed, the stacks empty, READY. */
[[noreturn]]
void flow_end(Session *S);
/* Illegal direct when running the direct line. */
void flow_nodir(Session *S);
/* Each op's size, 0 one that says its own */
extern const uint8_t flow_op_sizes[O_NOPS];
/* The size of the op at op, with its operands. */
static inline uint32_t flow_opsize(const uint8_t *const op)
{
    if (flow_op_sizes[op[0]])
        return flow_op_sizes[op[0]];
    if (op[0] == O_ON)
        return (op[1] & ON_COUNT) * sizeof(LineRef) + 2;

    return op[1] + 2u; /* its length byte */
}
/* A FUNCTION's result between its return and its push. A string's descriptor is in the arena,
   where a heap block's owner must be */
typedef struct {
    Addr text;
    Cell value;
} Flow;

#endif
