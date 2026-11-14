/* Control flow: lines, jumps, GOSUB, FOR and NEXT, SUB and FUNCTION calls, ON ERROR and RESUME.

   GOSUBs, events, FOR loops and calls are Frames on vm.frames. A call sets its parameters and
   locals aside on vm.callvars. */
#ifndef CORE_VM_FLOW_H
#define CORE_VM_FLOW_H
#include "vm/exec.h"

enum : uint32_t { LINE_END = 0xFFFFFFFF }; /* a Place's code: the start of the line after */

/* The FUNCTION result's string descriptor carved from the arena. */
void flow_init(Session *S);

/* Push a frame of kind, returning where the running statement goes on. Out of stack space. */
Frame *flow_push(Session *S, FrameKind kind);
/* The innermost frame of kind, nullptr none. */
Frame *flow_find(Session *S, FrameKind kind);
/* The place of vm->ip in the running line. */
Place flow_here(Session *S);
/* The code at place, its line compiled if the cache dropped it. */
uint8_t *flow_place(Session *S, Place place);

/* Go to line's start, linking the program first if it is not. */
void flow_jump(Session *S, uint32_t line);
/* Go to the next line; past the last the program ENDs. */
void flow_nextln(Session *S);
/* The LineRef operand at vm->ip: its line, found by number on first use. Undefined line number. */
uint32_t flow_target(Session *S);
/* The direct line compiled into the cache; returns its code. Out of memory. */
uint8_t *flow_dcode(Session *S);

/* The handler for an error now: the innermost call's ON LOCAL ERROR, the calls above it returned
   as if the error were at their call; else ON ERROR's. 0 none. */
uint32_t flow_handler(Session *S);
/* Run handler for error code: sets ERR, ERL and RESUME's places, then back into the loop. */
[[noreturn]]
void flow_trap(Session *S, uint8_t code, uint32_t handler);
/* ERL: the last line number at or before line, 0 none. */
uint16_t flow_erl(Session *S, uint32_t line);
/* The statement map entry of the statement holding code in line; *left the entries from it on. */
Stmt *flow_stmtof(Session *S, uint32_t line, const uint8_t *code, uint32_t *left);

/* CONT goes on at the running statement; not from the direct line. */
void flow_setcont(Session *S);
/* The program ends: files closed, the stacks empty, READY. */
[[noreturn]]
void flow_end(Session *S);
/* Illegal direct when running the direct line. */
void flow_nodir(Session *S);
/* The size of the op at op, with its operands. */
uint32_t flow_opsize(const uint8_t *op);
/* Move the string descriptor at from to to, which then owns its heap block. */
void flow_lsmove(Session *S, Addr from, Addr to);
/* A FUNCTION's result between its return and its push. A string's descriptor is in the arena,
   where a heap block's owner must be */
typedef struct {
    Addr text;
    Cell value;
} Flow;

#endif
