/* Errors and the READY loop's interface. How it works: repl.c. */
#ifndef CORE_IO_REPL_H
#define CORE_IO_REPL_H
#include <setjmp.h>
#include "defs.h"
#include "fault.h"
#include "comp/lexer.h"
#include "plat/pfil.h"

/* setjmp(*repl.top)'s values */
enum { REPL_CALLED = 0, REPL_READY = 1, REPL_ERROR = 2 };

/* repl_enter's result */
enum : uint8_t { ENTER_DIRECT = 0, ENTER_EMPTY = 1, ENTER_STORED = 2 };

/* A front end's part at the READY loop's places; a null entry, or one that returns: the
   console's. */
typedef struct {
    void (*regions)(Session *S); /* boot's: its arena blocks, before the variables' */
    void (*start)(Session *S);   /* under repl_call, before the READY loop */
    void (*chain)(Session *S);   /* CHAIN begins: may raise */
    void (*warn)(Session *S, const char *text, uint32_t len); /* a load warning, its CR LF too */
    uint32_t
        heap; /* the C side's heap's bytes on DOS, 0 C_HEAP: what statics held, the arena keeps */
} FrontEnd;

enum { C_HEAP = 0x60000 }; /* the batch's C side: the compiler's and the run's buffers */

typedef struct {
    const FrontEnd *fe; /* boot's */
    jmp_buf *top;       /* the innermost repl_call's: every unwind but a trap's and a stage's */
    uint8_t err;        /* REPL_ERROR's error, E_* */
    uint16_t fline;     /* loading a file: its line, for a warning */
    bool loading;       /* loading a file: every line is stored, at the end */
    bool batch;         /* a file run: its end exits */
    char file[PFIL_PATH]; /* the command line's file name; CHAIN's */
    char incl[PFIL_PATH]; /* an '$INCLUDE's */
} Repl;

/* Raise error code (E_*): never returns (above). */
[[noreturn]]
void repl_error(Session *S, uint8_t code);

/* basic32's: the file the command line names run, then exit; warnings to standard error. */
extern const FrontEnd repl_batch;

/* The start (repl.fe's), then the READY loop: READY., lines read and entered. */
[[noreturn]]
void repl_main(Session *S);
/* fn under its own boundary (repl.top): ST_OK when it returns or a run it starts ends (READY);
   else the error that ended it, the stacks reset. */
Status repl_call(Session *S, void (*fn)(Session *S));
/* Session.names' intern: the symbol table's name. */
uint16_t repl_intern(LexrNames *names, const uint8_t *text, uint8_t len);
/* The line's declarations entered, after its 32-bit warnings to the front end. */
void repl_declare(Session *S, const uint8_t *body);
/* Session.fault's raise: a module's error, reported as repl_error does, at its line when the block pass found it. */
[[noreturn]]
void repl_fault(Fault *f, const Failure *e);
/* Back to READY (asm rpl_ready): the C stack unwound to repl.top, the run ended. */
[[noreturn]]
void repl_ready(Session *S);
/* Run the direct line repl_enter left in comp_cbuf: under repl_call, which it ends. */
[[noreturn]]
void repl_run(Session *S);
/* CONT from the editor: as repl_run. */
[[noreturn]]
void repl_cont(Session *S);
/* Enter the lines of the file repl.file names, its includes' in place; then the console's
   again. Errors: as chnl_open. */
void repl_load(Session *S);
/* Tokenize inbuf, len long; store a numbered line, an empty one deletes it; loading, every line
   at the end. ENTER_*. Errors: Syntax error a number 64000 or over, Out of memory, Duplicate
   label. */
uint8_t repl_enter(Session *S, uint32_t len);
/* The stored lines entered again, as a loaded file's: declarations, labels, variables new;
   an include's lines stored after its line (LF_INC), those of before dropped first. */
void repl_relink(Session *S);
#endif
