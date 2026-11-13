/* The name table and the variables (src/mem/sym.asm).

   A name, 1 .. nnames-1, as QBasic's name table: a Name in names, its text in text, its length
   then its letters in upper case, a suffix left out. A variable, 1 .. nvars-1, as QBasic's
   variable manager: a name, a type (ET_*) and whether an array. vars holds per variable its 8-byte
   cell, a Var and a VarX. Var.vt is a scalar's cell (its own, or a parameter's argument's while
   its procedure runs), an array's header. Compiled code holds cells' Addrs: the cells move only
   when an edit drops the code. Each table chains its hashes from a head, 0 ends a chain. */
#ifndef MEM_SYMB_H
#define MEM_SYMB_H
#include "tabl.h"

/* A variable's info: its kind in bits 1-0 (comp's g_etk), SK_DBL, VI_ARR. */
enum : uint8_t { VK_FLT = 0, VK_INT = 1, VK_STR = 2, VK_MASK = 3, VI_ARR = 0x80 };

typedef struct PACKED {
    Addr vt;       /* its cell; an array's header, 0 none */
    uint16_t name;
    uint8_t type;  /* ET_* */
    uint8_t info;  /* VI_ARR, SK_DBL, the kind */
} Var;             /* TVAR */
typedef struct PACKED {
    uint16_t next;  /* its hash chain's next */
    uint16_t nloc;  /* the next local of its procedure */
    uint8_t scope;  /* 0 the module, else a procedure */
    uint8_t dyn;    /* REDIM made it; ERASE frees it */
    uint16_t pad;
} VarX;            /* TVX */
typedef struct PACKED {
    uint32_t text; /* its text's offset in text: its length, then its letters */
    uint8_t flags; /* NM_* */
    uint8_t rec;   /* its TYPE, 0 none */
    uint8_t proc;  /* its procedure, 0 none */
    uint8_t pad;
} Name;            /* TNAM */
static_assert(sizeof(Var) == 8 && sizeof(VarX) == 8 && sizeof(Name) == 8);

typedef struct {
    Table vars;                  /* the cells, Var, VarX */
    Table names;                 /* Name, the chains' next (uint16_t) */
    Table text;                  /* the names' text */
    uint32_t text_free;          /* text's next byte, an offset (sym.tfree) */
    uint16_t nvars;              /* the next variable (lx.nsyms) */
    uint16_t nnames;             /* the next name (lx.nnams) */
    uint16_t first_local[NPROC]; /* a procedure's first local, VarX.nloc on (g_plh) */
    uint8_t deft[26];            /* DEFtype's ET_*, by first letter (g_deft) */
} Symb;
extern Symb symb;

static inline Addr symb_cell(uint16_t id) { return symb.vars.at[0] + id * 8u; }
static inline Var *symb_var(uint16_t id) { return (Var *)at(symb.vars.at[1]) + id; }
static inline VarX *symb_varx(uint16_t id) { return (VarX *)at(symb.vars.at[2]) + id; }
static inline Name *symb_name(uint16_t id) { return (Name *)at(symb.names.at[0]) + id; }
/* Name id's text: its length, then its letters. */
static inline const uint8_t *symb_text(uint16_t id)
{
    return at(symb.text.at[0] + symb_name(id)->text);
}

/* The tables' blocks, after the program store's. */
void symb_new(void);
/* Room for the program's variables before it runs (each M_VAR byte in the store counts one): they
   are made lazily, when the heap may hold strings and arrays and the block cannot grow. */
void symb_size(void);
/* CLR: variables zeroed, arrays dropped, READ reset, the heap and the temporaries emptied. */
void symb_clear(void);
/* No names, no variables, no declarations. */
void symb_init(void);
/* No variables; the names kept. */
void symb_vars_init(void);
/* The name of text[0..len) (no suffix), made when new (NM_FN when it starts FN). Out of memory:
   past 65535 names, or the heap not empty. */
uint16_t symb_find_name(const uint8_t *text, uint8_t len);
/* The variable of name, type (ET_*) and kind (VI_ARR, the low bits as info's) in scope (0 the
   module, else a procedure), made when new, a scalar zeroed; a new one of a procedure joins its
   locals unless stat. Out of memory: past 65535 variables, or the heap not empty. */
uint16_t symb_find_var(uint16_t name, uint8_t type, uint8_t kind, uint8_t scope, bool stat);
#endif
