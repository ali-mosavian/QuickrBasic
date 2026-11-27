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
    uint32_t scope;  /* a module's (sc_module), a procedure's name; | SC_NUM a line number's */
    uint32_t name;   /* the name, or the line number */
    Binding *next;   /* its hash chain's next */
    uint16_t flags;  /* BF_* */
    uint8_t shapes;  /* BS_*: the shapes a DIM or parameter declared; in flags' padding */
    uint32_t et[2];  /* the declared type: the scalar's, the array's */
    uint32_t rec[2]; /* the record's TYPE: the scalar's, the array's */
    uint16_t fix[2]; /* STRING * n: the scalar's, the array's */
    uint32_t line;   /* a label's line */
    int32_t val;     /* BF_VAL: a CONST's value; BF_PROC: its verdict (comp/export.h) */
};

/* Binding.shapes: a scalar's, an array's. */
enum : uint8_t { BS_SCALAR = 1, BS_ARRAY = 2 };

enum {
    NSCS = 4,
    SCHASH = 1024,
    NLOOK = 512, /* scop.look's answers */
};

/* A scope on the stack: its name (a module's key at the bottom), and whether a DEF FN's. */
typedef struct {
    uint32_t name;
    bool deffn;
} ScopFrame;

/* A scop_look answer kept: its key (name, the stack's top and depth), its gen. */
typedef struct {
    uint32_t name, top, depth, gen;
    const Binding *b;
    uint32_t scope;
} ScopLook;

typedef struct {
    Arena arena; /* the bindings */
    Fault *fault;
    uint32_t nbnd;
    uint32_t depth; /* the stack's scopes */
    ScopFrame stack[NSCS];
    Binding **heads; /* SCHASH, from the arena */
    /* scop_look's answers by name and the stack (its depth and top), NLOOK of them, while a
       link compiles (comp/link.c), which binds nothing: scop_look_on to scop_look_off, nullptr
       else. gen counts the bindings made and the answers' starts, an answer of another gen
       stale. A link asked 12K times. */
    uint32_t gen;
    ScopLook *look;
} Scop;

/* scop_look's answers kept from here on, none yet: their room taken. Out of memory. */
void scop_look_on(Scop *sc);
/* No answers kept: their room given back. */
void scop_look_off(Scop *sc);
/* Errors raise through f. No bindings. */
void scop_init(Scop *sc, Fault *f);
/* No bindings. */
void scop_reset(Scop *sc);
/* Scope's binding of name, nullptr none. */
Binding *scop_get(const Scop *sc, uint32_t scope, uint32_t name);
/* Scope's binding of name, made (zeroed) when none. Out of memory. */
Binding *scop_bind(Scop *sc, uint32_t scope, uint32_t name);
/* The stack: module's scope (sc_module), then procedure p (null none), which is module's. */
void scop_enter(Scop *sc, uint32_t module, const Proc *p);
/* Name's scope pushed on the stack: a line checked alone in the procedure being entered. */
void scop_inside(Scop *sc, uint32_t name);
/* The scope a new name is made in: the top one not a DEF FN's. */
uint32_t scop_own(const Scop *sc);
/* Name's binding from the top of the stack down, a zero one for none; *scope its scope, for none
   scop_own's. */
const Binding *scop_look(const Scop *sc, uint32_t name, uint32_t *scope);
/* Name's declared type (ET_*), record TYPE and STRING * n, the array's when array, as the
   procedure compiled sees it; 0 none. */
uint32_t scop_type(const Scop *sc, uint32_t name, bool array);
uint32_t scop_rec(const Scop *sc, uint32_t name, bool array);
uint16_t scop_fix(const Scop *sc, uint32_t name, bool array);
/* Name used with suffix type (ET_*), the array when array: a CONST's type for a CONST, else type.
   Duplicate definition (E_RD): DIM AS another type, or a record. */
uint32_t scop_sfx(const Scop *sc, uint32_t name, uint8_t type, bool array);
/* Name a CONST, as the procedure compiled sees it? */
bool scop_con(const Scop *sc, uint32_t name);
/* Name a CONST's: the module's (every procedure's), or the procedure compiled's own? */
bool scop_conname(const Scop *sc, uint32_t name);
/* CONST name's integer value as procedure proc (0 none) of module sees it, its own else the
   module's: *v. false: none known (comp/declare.c decl_const). */
bool scop_conval(const Scop *sc, uint32_t module, uint32_t proc, uint32_t name, int32_t *v);
/* Name bound, as the procedure compiled sees it? */
bool scop_known(const Scop *sc, uint32_t name);
/* Name known as a scalar: not only its array declared (BS_ARRAY alone). */
bool scop_known_scalar(const Scop *sc, uint32_t name);
/* Name a variable module shares: DIM SHARED, not a CONST? */
bool scop_mshr(const Scop *sc, uint32_t module, uint32_t name);
/* Label key (a name, or a line number when scope has SC_NUM) of scope, which is module's, at
   line. Duplicate label (E_DL): any scope of module has it. */
void scop_label(Scop *sc, Symb *sy, uint32_t module, uint32_t scope, uint32_t key, uint32_t line);
/* Label key of scope, which is module's: its line, 0 when no scope of module has it. Subprogram
   error (E_SPE): another scope's. */
uint32_t scop_lfind(const Scop *sc, uint32_t module, uint32_t scope, uint32_t key);
#endif
