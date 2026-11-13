/* The scoped declarations (src/mem/scope.asm): a binding is a name's in one scope, the module's (0)
   or a procedure's (its name), holding the declared type, record and STRING * n of the scalar and
   of the array, a label's line and BF_* flags. A procedure's binding, any but BF_USE alone, makes
   the name its own there. A label is a binding too; a line number's is in its scope | SC_NUM, keyed
   by the number. The compiler's scopes are a stack of procedures, the module at its bottom; a name
   is looked up from the top down. From a SUB or FUNCTION the module's names are seen only when
   BF_SHR, or BF_REF there; from a DEF FN all. */
#ifndef MEM_SCOP_H
#define MEM_SCOP_H
#include "defs.h"

typedef struct PACKED {
    uint16_t scope;  /* 0 the module, a procedure's name; | SC_NUM a line number's */
    uint16_t name;   /* the name, or the line number */
    uint16_t next;   /* its hash chain's next, 0 none */
    uint8_t flags;   /* BF_* */
    uint8_t et[2];   /* the declared type: the scalar's, the array's */
    uint8_t rec[2];  /* the record's TYPE: the scalar's, the array's */
    uint8_t pad;
    uint16_t fix[2]; /* STRING * n: the scalar's, the array's */
    uint32_t line;   /* a label's line */
} Binding;
static_assert(sizeof(Binding) == 20);

/* No bindings. */
void scop_init(void);
/* Scope's binding of name, nullptr none. */
Binding *scop_get(uint16_t scope, uint16_t name);
/* Scope's binding of name, made (zeroed) when none. Out of memory past NBND. */
Binding *scop_bind(uint16_t scope, uint16_t name);
/* Procedure proc compiled (sets comp's cproc), 0 the module: the stack the module, then it. */
void scop_proc(uint8_t proc);
/* The scope a new name is made in: the top one not a DEF FN's. */
uint16_t scop_own(void);
/* Name's binding from the top of the stack down, a zero one for none; *scope its scope, for none
   scop_own's. */
const Binding *scop_look(uint16_t name, uint16_t *scope);
/* Name's declared type (ET_*), record TYPE and STRING * n, the array's when array, as the
   procedure compiled sees it; 0 none. */
uint8_t scop_type(uint16_t name, bool array);
uint8_t scop_rec(uint16_t name, bool array);
uint16_t scop_fix(uint16_t name, bool array);
/* Name used with suffix type (ET_*), the array when array: a CONST's type for a CONST, else type.
   Duplicate definition (E_RD): DIM AS another type, or a record. */
uint8_t scop_sfx(uint16_t name, uint8_t type, bool array);
/* Name a CONST, as the procedure compiled sees it? */
bool scop_con(uint16_t name);
/* Name bound, as the procedure compiled sees it? (The asm's CF is the opposite.) */
bool scop_known(uint16_t name);
/* Name a variable the module shares: DIM SHARED, not a CONST? */
bool scop_mshr(uint16_t name);
/* Label key (a name, or a line number when scope has SC_NUM) of scope at line. Duplicate label
   (E_DL): any scope has it. */
void scop_label(uint16_t scope, uint16_t key, uint32_t line);
/* Label key of scope: its line, 0 when no scope has it. Subprogram error (E_SPE): another
   scope's. */
uint32_t scop_lfind(uint16_t scope, uint16_t key);
enum {
    NBND = 16384, /* numbered lines' too */
    NSCS = 4,
    SCHASH = 1024,
    WORD_BITS = 32,
};

typedef struct {
    uint32_t nbnd;
    uint32_t depth;                          /* the stack's scopes */
    uint16_t stack[NSCS];                    /* procedures, 0 the module */
    uint16_t heads[SCHASH];
    uint32_t lnbit[LNUM_LIMIT / WORD_BITS]; /* the line numbers a scope has */
    Binding bnd[NBND];
} Scop;
extern Scop scop;

#endif
