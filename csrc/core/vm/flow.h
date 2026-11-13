/* The executor's control flow (src/vm/flow.asm): lines, jumps, GOSUB, FOR and NEXT, procedures,
   ON ERROR and RESUME.

   The control stack holds frames (defs.h) from exec.cstk up to allo.cs_top, a frame's mark its
   size: GOSUB's and an event's, FOR's, a procedure's. A frame's resume is a code offset (exec.h).
   A procedure's call (PCALL) keeps its arguments and its locals' values on the save stack, LSSZ
   bytes an entry: id:2, ref:2 (FFFFh none, FFFEh a scalar parameter's cell), the value:8, a
   scalar's cell before:4. */
#ifndef CORE_VM_FLOW_H
#define CORE_VM_FLOW_H
#include "vm/exec.h"

/* ex_line, ex_dcode: the direct line's code, compiled from comp's xbuf, as a stream at DCODE;
   exec.ip past its statement map. Out of memory: it does not fit. */
uint8_t *flow_dcode(void);
/* ex_end: the run is over, both stacks empty, READY; back to dbug's probe if it ran. */
[[noreturn]] void flow_end(void);
/* ex_erl: ERL's number: the last line number at or before line, 0 none. */
uint16_t flow_erl(uint32_t line);
/* ex_gsf: push a GOSUB-like frame of mark (M_GS, M_EV), RETURN going on at exec.line, exec.ip.
   Out of stack space. */
Addr flow_push(uint8_t mark);
/* ex_hnd: the error's handler line, 0 none: the innermost ON LOCAL ERROR's, the procedures its
   procedure called dropped as by their returns (exec.line, exec.opip then the call); else ON
   ERROR's. */
uint32_t flow_handler(void);
/* ex_trap: the error code ON ERROR handles at handler: ERR, ERL, where RESUME goes on, then the
   handler run on the failing procedure's data stack, frames kept. Into exec_run's loop. */
[[noreturn]] void flow_trap(uint8_t code, uint32_t handler);
/* ex_jump: on at line's start; the program linked first if it is not. */
void flow_jump(uint32_t line);
/* ex_nextln, the NXLN op's: on to the next line (a false IF); the program's end ENDs. */
void flow_nextln(void);
/* ex_nodir: Illegal direct in the direct line. */
void flow_nodir(void);
/* ex_resolve: code offset off of line's code (compiled if it has none) as an address. */
uint8_t *flow_resolve(uint32_t line, uint32_t off);
/* ipload: a frame's or CONT's place: off from the cache's base, or under DCODE in line's code. */
uint8_t *flow_place(uint32_t line, uint32_t off);
/* ex_setcont: CONT goes on at exec.line, exec.ip; not for the direct line. */
void flow_setcont(void);
/* ex_stmtof: line's statement map entry of the statement holding code: the last at it or
   before, else the first; *left the entries from it on, the line's EOL's last. */
Stmt *flow_stmtof(uint32_t line, const uint8_t *code, uint32_t *left);
/* ex_target: the LineRef at exec.ip, its line, found and kept the first time; exec.ip past it.
   Undefined line number. */
uint32_t flow_target(void);
/* ls_mov: the Desc at from to the Desc at to, which then owns its heap block. */
void flow_lsmove(Addr from, Addr to);
/* op_size: the bytes of the op at op. */
uint32_t flow_opsize(const uint8_t *op);
#endif
