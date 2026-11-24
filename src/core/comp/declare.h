/* Declarations' compiler's interface. How it works: declare.c. */
#ifndef COMP_DECLARE_H
#define COMP_DECLARE_H
#include "defs.h"
#include "fault.h"
#include "mem/scope.h"
#include "mem/symbols.h"
#include "util/btab.h"

enum { NDEV = 256, NLETTERS = 26 };

/* A fixed array field's dimension, ours: its lower bound, its elements. */
typedef struct {
    int32_t lb;
    uint32_t count;
} FieldDim;

/* A field of a TYPE. */
typedef struct {
    uint32_t name;
    uint32_t et, sub; /* its ET_*, or its TYPE's; an array's, its element's */
    uint32_t off;
    uint16_t fix;
    uint8_t ndim; /* ours: a fixed array's dimensions, 0 none */
    bool dyn;     /* ours: a dynamic array's cell (ArrSlot), empty until REDIM */
    uint16_t dim; /* its first in Decl.dims */
} Field;

/* A TYPE: its name, its fields, its bytes as QBasic lays them; ours, its STRINGs' Desc offsets and
   its dynamic arrays' cells', its records' and fixed arrays' too. */
typedef struct {
    uint32_t name;
    uint16_t nfld;
    uint32_t size; /* TYPE_MAX at most, ours; QBasic's 65535 */
    uint32_t cap;
    Field *fields;
    uint32_t nstr, strcap;
    uint32_t *strs;
    uint32_t nslot, slotcap;
    uint32_t *slots; /* ours: its dynamic arrays' cells' offsets, its records' too */
} TypeDef;
/* A TYPE's bytes at most, ours: GET's and PUT's sizes are 24 bits (defs.h va_pack). */
enum : uint32_t { TYPE_MAX = 0xFFFFFF };

/* A DEFtype line: the DEFtypes from it on. */
typedef struct {
    uint32_t line;
    uint8_t deft[NLETTERS];
} DefEvent;

/* A COMMON block's variable, at its place in it: the first module's name for it, which the others'
   stand for. */
typedef struct {
    uint32_t block; /* its block's name, the main's; 0 the blank block */
    uint32_t name;
    uint32_t type;  /* ET_*, ET_REC + t */
    uint32_t scope; /* the last module to name it, sc_module's */
    uint16_t fix;   /* STRING * n's n */
    bool array;
} CmnVar;

/* A file's OPTIONs: OPTION BASE 1, OPTION EXPLICIT. */
enum : uint8_t { DO_BASE1 = 1, DO_EXPLICIT = 2 };

/* The program's declarations; its arena holds the tables. */
typedef struct {
    Arena arena;
    Fault *fault;
    uint32_t module; /* the module being entered: its scope (sc_module) */
    uint32_t top;    /* the first module's first line, LF_TOP: the main's end; ~0 none */
    uint32_t proc;   /* lx.dproc: the procedure being entered, its name; 0 none */
    uint32_t ntypes; /* lx.ntyps: the last TYPE's index */
    uint32_t type;   /* lx.dtype: the TYPE being entered, 0 none */
    uint32_t scope;  /* g_dscope: where declarations go */
    bool redim;      /* g_dredim: the DIM is a REDIM */
    bool baseset;    /* g_obset: the module's OPTION BASE given */
    bool dimmed;     /* g_odim: an array DIMmed (OPTION BASE after: an error) */
    bool deft;       /* decl_line's line has a DEFtype */
    bool native;     /* the module being entered is a .qbx's interface: its DECLARE CDECLs native */
    uint8_t *opts;   /* g_obase, g_oexp: each module's DO_*, by its number */
    uint32_t nopts, optcap;
    uint32_t ndefev, defcap; /* g_ndev: DEFtype lines */
    DefEvent *defevs;
    BTab types; /* TypeDef, g_typ: by number, from 1 */
    uint32_t ndims, dimcap;
    FieldDim *dims; /* the fixed array fields' dimensions */
    uint32_t ncmn, cmncap;
    CmnVar *cmn; /* COMMON's variables, each block's in order */
} Decl;

extern const uint8_t decl_astype[5]; /* g_dast: AS INTEGER .. AS STRING to ET_* */
extern const uint8_t decl_etsize[8]; /* g_etsz: a field's bytes by ET_* */

/* Ours, FFh n: AS n's ET_* (SHORT, BYTE), DEFn's (DEFSHORT, DEFBYTE); 0 none. */
uint8_t decl_xastype(uint8_t tk1);
uint8_t decl_xdeftype(uint8_t tk1);

/* Errors raise through f. */
void decl_init(Decl *d, Fault *f);
/* No declarations; the names kept. */
void decl_reset(Decl *d, Scop *sc, Symb *sy);
/* Enter the body's declarations, of the line at line; loading: from a file. */
void decl_line(Decl *d, Scop *sc, Symb *sy, const uint8_t *body, uint32_t line, bool loading);
/* The lines entered next module's (sc_module), from line on, the LF_TOP before an imported one's:
   its file's DEFtypes and OPTIONs start again. */
void decl_module(Decl *d, Scop *sc, Symb *sy, uint32_t module, uint32_t line);
/* Module's file's DO_*. */
static inline uint8_t decl_opts(const Decl *const d, const uint32_t module)
{
    const uint32_t m = module & ~SC_MOD;
    return m < d->nopts ? d->opts[m] : 0;
}
/* Module's file's OPTION BASE. */
static inline uint8_t decl_base(const Decl *const d, const uint32_t module)
{
    return decl_opts(d, module) & DO_BASE1;
}
/* Each DEFtype line's event, again from the program pg in its order: a line typed, or put
   between others, has its place. */
void decl_defevents(Decl *d, Scop *sc, Symb *sy, const Prog *pg);
/* dc_deft: the DEFtypes (sy->deft) line sees. */
void decl_deftype(const Decl *d, Symb *sy, uint32_t line);
/* Ours, for a front end of its own (qr/qlink.c): a new TYPE of name (a symbol table name), its
   number; then its fields in order, each of ET_* or ET_REC + t et, count elements (0 one), dyn a
   dynamic array's cell. Out of memory. */
uint32_t decl_newtype(Decl *d, Symb *sy, uint32_t name);
void decl_newfield(Decl *d, uint32_t t, uint32_t name, uint32_t et, uint32_t count, bool dyn);
/* TYPE t. */
static inline const TypeDef *decl_type(const Decl *const d, const uint32_t t)
{
    return btab_at(&d->types, t);
}
/* The next statement from *at with no effect in 32-bit mode (DEF SEG, VARSEG, SSEG): its name,
   *at past it; null none. */
const char *decl_warning(const uint8_t **at);
const uint8_t *decl_item(const uint8_t *p);  /* dc_item: past the item at p */
const uint8_t *decl_paren(const uint8_t *p); /* past the ( .. ) at p; p itself when none */
/* A TYPE's array field's ( [lo TO] hi {, ..} ) at p, ours, each bound an integer constant (the
   CONSTs as procedure proc, 0 none, of module sees them), base the lower one's when none: its
   dimensions to dims, MAXDIMS at most, their count *n, 0 for (), a dynamic one's; past it, null
   not one, *n past MAXDIMS for too many. */
const uint8_t *decl_dims(
    const Scop *sc,
    uint32_t module,
    uint32_t proc,
    const uint8_t *p,
    uint8_t base,
    FieldDim *dims,
    uint8_t *n
);
/* TYPE td holds STRINGs or dynamic arrays, ours: its bytes are not all its data. */
static inline bool decl_dynamic(const TypeDef *const td) { return td->nstr || td->nslot; }
/* Field f's elements' type as a variable's: ET_*, ET_REC + t a TYPE's. */
static inline uint32_t decl_fieldtype(const Field *const f)
{
    return f->sub ? ET_REC + f->sub : f->et;
}
/* Field i of TYPE t's key, ours, a fixed array field's for the run (vm/exec.c exec_arrayop); t
   FIELD_KEY_TYPES at most. */
enum : uint32_t { FIELD_KEY_TYPES = 0x7FFF };
static inline uint32_t decl_fieldkey(const uint32_t t, const uint32_t i) { return t << 16 | i; }
/* The field a key names. */
static inline const Field *decl_keyfield(const Decl *const d, const uint32_t key)
{
    return &decl_type(d, key >> 16)->fields[key & 0xFFFF];
}
/* Field f's dimension k. */
static inline const FieldDim *decl_fielddim(
    const Decl *const d,
    const Field *const f,
    const uint32_t k
)
{
    return &d->dims[f->dim + k];
}
/* dc_proc: the body is a SUB's or FUNCTION's first line (PK_SUB, PK_FUNC; *name its M_VAR), their
   END (PK_END), else 0. */
uint8_t decl_proc(const uint8_t *body, const uint8_t **name);
/* The lines declared from here are procedure proc's (its name), 0 the module's. */
void decl_enter(Decl *d, uint32_t proc);
/* The lines declared from here are the main module's, its end's DEFtypes theirs. */
void decl_main(Decl *d, Symb *sy);
bool decl_tail(const uint8_t **at); /* *at the body to its last statement; true ends in a string,
                                       REM's or DATA's text */
const uint8_t *decl_step(const uint8_t *p); /* decl_item, a number kept as text whole */
#endif
