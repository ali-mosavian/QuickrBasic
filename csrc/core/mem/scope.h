/* The scoped declarations' interface. How it works: scope.c. */
#ifndef MEM_SCOPE_H
#define MEM_SCOPE_H
#include "defs.h"
#include "fault.h"
#include "comp/blocks.h"
#include "mem/symbols.h"
#include "util/arena.h"

typedef struct Binding Binding;
struct Binding {
    uint16_t scope;  /* 0 the module, a procedure's name; | SC_NUM a line number's */
    uint16_t name;   /* the name, or the line number */
    Binding *next;   /* its hash chain's next */
    uint8_t flags;   /* BF_* */
    uint8_t et[2];   /* the declared type: the scalar's, the array's */
    uint8_t rec[2];  /* the record's TYPE: the scalar's, the array's */
    uint16_t fix[2]; /* STRING * n: the scalar's, the array's */
    uint32_t line;   /* a label's line */
};

enum {
    NBND = 16384, /* numbered lines' too */
    NSCS = 4,
    SCHASH = 1024,
    WORD_BITS = 32,
};

/* A scope on the stack: its name (0 the module), and whether a DEF FN's. */
typedef struct {
    uint16_t name;
    bool deffn;
} ScopFrame;

typedef struct {
    Arena arena; /* the bindings */
    Fault *fault;
    uint32_t nbnd;
    uint32_t depth; /* the stack's scopes */
    ScopFrame stack[NSCS];
    Binding **heads; /* SCHASH, from the arena */
    uint32_t *lnbit; /* the line numbers a scope has: nlnbit words, grown as numbers come */
    uint32_t nlnbit;
} Scop;

/* Errors raise through f. No bindings. */
void scop_init(Scop *sc, Fault *f);
/* No bindings. */
void scop_reset(Scop *sc);
/* Scope's binding of name, nullptr none. */
Binding *scop_get(const Scop *sc, uint16_t scope, uint16_t name);
/* Scope's binding of name, made (zeroed) when none. Out of memory past NBND. */
Binding *scop_bind(Scop *sc, uint16_t scope, uint16_t name);
/* The stack: the module, then procedure p (null none). */
void scop_enter(Scop *sc, const Proc *p);
/* The scope a new name is made in: the top one not a DEF FN's. */
uint16_t scop_own(const Scop *sc);
/* Name's binding from the top of the stack down, a zero one for none; *scope its scope, for none
   scop_own's. */
const Binding *scop_look(const Scop *sc, uint16_t name, uint16_t *scope);
/* Name's declared type (ET_*), record TYPE and STRING * n, the array's when array, as the
   procedure compiled sees it; 0 none. */
uint8_t scop_type(const Scop *sc, uint16_t name, bool array);
uint8_t scop_rec(const Scop *sc, uint16_t name, bool array);
uint16_t scop_fix(const Scop *sc, uint16_t name, bool array);
/* Name used with suffix type (ET_*), the array when array: a CONST's type for a CONST, else type.
   Duplicate definition (E_RD): DIM AS another type, or a record. */
uint8_t scop_sfx(const Scop *sc, uint16_t name, uint8_t type, bool array);
/* Name a CONST, as the procedure compiled sees it? */
bool scop_con(const Scop *sc, uint16_t name);
/* Name bound, as the procedure compiled sees it? */
bool scop_known(const Scop *sc, uint16_t name);
/* Name a variable the module shares: DIM SHARED, not a CONST? */
bool scop_mshr(const Scop *sc, uint16_t name);
/* Label key (a name, or a line number when scope has SC_NUM) of scope at line. Duplicate label
   (E_DL): any scope has it. */
void scop_label(Scop *sc, Symb *sy, uint16_t scope, uint16_t key, uint32_t line);
/* Label key of scope: its line, 0 when no scope has it. Subprogram error (E_SPE): another
   scope's. */
uint32_t scop_lfind(const Scop *sc, const Symb *sy, uint16_t scope, uint16_t key);
#endif
