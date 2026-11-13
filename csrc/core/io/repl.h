/* Errors and the READY loop (io/repl.asm). repl_error is the asm's err_raise: it never returns,
   it unwinds with longjmp to the first of these that holds:
     1. dbug's watch probe running (g_dbev): back to it (dbg_ever).
     2. compiling (comp.busy): comp_fail, which ends the line with an ERR op and longjmps to
        its compile's start.
     3. not BREAK nor E_OS, not in the handler, not the direct line, and a handler found
        (flow's ex_hnd): flow_trap (ex_trap), which longjmps to vm.loop at the handler.
     4. else repl.top with REPL_ERROR: the message (and a file run's where, a compile error's
        place and words), then READY, the batch exit, or the editor's ui_err.
   Before any of them: lexr.more is cleared, the x87 cleared (ex_fclr). repl.top is where the
   asm reset ESP to rn.rsp: the C stack's floor. */
#ifndef CORE_IO_REPL_H
#define CORE_IO_REPL_H
#include <setjmp.h>
#include "defs.h"

/* setjmp(repl.top)'s values */
enum { REPL_START = 0, REPL_READY = 1, REPL_ERROR = 2 };

/* repl_enter's result */
enum : uint8_t { ENTER_DIRECT = 0, ENTER_EMPTY = 1, ENTER_STORED = 2 };

typedef struct {
    jmp_buf top;        /* the READY loop's: every unwind but a trap's and a compile's */
    uint8_t err;        /* REPL_ERROR's error, E_* */
    uint16_t fline;     /* loading a file: its line, for a warning */
    bool loading;       /* loading a file: every line is stored, at the end */
    bool batch;         /* a file run: its end exits */
    char file[128];     /* the command line's file name; CHAIN's */
    char incl[128];     /* an '$INCLUDE's */
} Repl;
extern Repl repl;

/* Raise error code (E_*): never returns (above). */
[[noreturn]] void repl_error(uint8_t code);
/* Raise QBasic's error qb as ERROR qb does. */
[[noreturn]] void repl_qberr(QbErr qb);

/* The start, then the READY loop: the command line's file run (the editor in the IDE), else
   READY., lines read and entered. repl.top's setjmp. */
[[noreturn]] void repl_main(void);
/* Back to READY (asm rpl_ready): the C stack unwound to repl.top. */
[[noreturn]] void repl_ready(void);
/* Run the direct line repl_enter left in comp_cbuf. */
[[noreturn]] void repl_run(void);
/* CONT from the editor. */
[[noreturn]] void repl_cont(void);
/* Enter the lines of the file repl.file names, its includes' in place; then the console's
   again. Errors: as chnl_open. */
void repl_load(void);
/* Tokenize inbuf, len long; store a numbered line, an empty one deletes it; loading, every line
   at the end. ENTER_*. Errors: Syntax error a number 64000 or over, Out of memory, Duplicate
   label. */
uint8_t repl_enter(uint32_t len);
/* The stored lines entered again, as a loaded file's: declarations, labels, variables new;
   an include's lines stored after its line (LF_INC), those of before dropped first. */
void repl_relink(void);
#endif
