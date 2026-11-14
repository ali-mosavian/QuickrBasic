/* The name table's and the variables' interface. How it works: symbols.c. */
#ifndef MEM_SYMBOLS_H
#define MEM_SYMBOLS_H
#include "table.h"

/* A variable's info: its kind in bits 1-0 (comp's g_etk), SK_DBL, VI_ARR. */
enum : uint8_t { VK_FLT = 0, VK_INT = 1, VK_STR = 2, VK_MASK = 3, VI_ARR = 0x80 };
/* A record array's info. */
enum : uint8_t { VI_RECARR = VI_ARR | VK_INT };

typedef struct PACKED {
    Addr vt; /* its cell; an array's header, 0 none */
    uint16_t name;
    uint8_t type; /* ET_* */
    uint8_t info; /* VI_ARR, SK_DBL, the kind */
} Var;            /* TVAR */
typedef struct PACKED {
    uint16_t next; /* its hash chain's next */
    uint16_t nloc; /* the next local of its procedure */
    uint8_t scope; /* 0 the module, else a procedure */
    uint8_t dyn;   /* REDIM made it; ERASE frees it */
    uint8_t kop;   /* a CONST's literal load, its operand in symb_kval; 0 none (comp/fold.h) */
    uint8_t pad;
} VarX; /* TVX */
typedef struct PACKED {
    uint32_t text; /* its text's offset in text: its length, then its letters */
    uint8_t flags; /* NM_* */
    uint8_t rec;   /* its TYPE, 0 none */
    uint8_t proc;  /* its procedure, 0 none */
    uint8_t pad;
} Name; /* TNAM */
static_assert(sizeof(Var) == 8 && sizeof(VarX) == 8 && sizeof(Name) == 8);

typedef struct {
    Table vars;                          /* the cells, Var, VarX, a CONST's literal (8 bytes) */
    Table names;                         /* Name, the chains' next (uint16_t) */
    Table text;                          /* the names' text */
    uint32_t text_free;                  /* text's next byte, an offset (sym.tfree) */
    uint16_t nvars;                      /* the next variable (lx.nsyms) */
    uint16_t nnames;                     /* the next name (lx.nnams) */
    uint16_t first_local[NPROC];         /* a procedure's first local, VarX.nloc on (g_plh) */
    uint8_t deft[26];                    /* DEFtype's ET_*, by first letter (g_deft) */
    uint16_t vhead[HSIZE], nhead[HSIZE]; /* the hash chains' heads */
} Symb;

static inline Addr symb_cell(const Symb *const sy, uint16_t id)
{
    return sy->vars.at[0] + id * (uint32_t)sizeof(Cell);
}
static inline Var *symb_var(const Symb *const sy, uint16_t id)
{
    return (Var *)at(sy->vars.at[1]) + id;
}
static inline VarX *symb_varx(const Symb *const sy, uint16_t id)
{
    return (VarX *)at(sy->vars.at[2]) + id;
}
static inline uint8_t *symb_kval(const Symb *const sy, uint16_t id)
{
    return at(sy->vars.at[3]) + id * 8u;
}
static inline Name *symb_name(const Symb *const sy, uint16_t id)
{
    return (Name *)at(sy->names.at[0]) + id;
}
/* Name id's text: its length, then its letters. */
static inline const uint8_t *symb_text(const Symb *const sy, uint16_t id)
{
    return at(sy->text.at[0] + symb_name(sy, id)->text);
}

/* The tables' blocks, after the program store's. */
void symb_new(Session *S);
/* Room for the program's variables before it runs (each M_VAR byte in the store counts one): they
   are made lazily, when the heap may hold strings and arrays and the block cannot grow. */
void symb_size(Session *S);
/* CLR: variables zeroed, arrays dropped, READ reset, the heap and the temporaries emptied. */
void symb_clear(Session *S);
/* No names, no variables, no declarations. */
void symb_init(Session *S);
/* No variables; the names kept. */
void symb_vars_init(Session *S);
/* The name of text[0..len) (no suffix), made when new (NM_FN when it starts FN). Out of memory:
   past 65535 names, or the heap not empty. */
uint16_t symb_find_name(Session *S, const uint8_t *text, uint8_t len);
/* The variable of name, type (ET_*) and kind (VI_ARR, the low bits as info's) in scope (0 the
   module, else a procedure), made when new, a scalar zeroed; a new one of a procedure joins its
   locals unless stat. Out of memory: past 65535 variables, or the heap not empty. */
uint16_t symb_find_var(
    Session *S,
    uint16_t name,
    uint8_t type,
    uint8_t kind,
    uint8_t scope,
    bool stat
);
#endif
