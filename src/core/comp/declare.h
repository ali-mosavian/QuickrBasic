/* Declarations' compiler's interface. How it works: declare.c. */
#ifndef COMP_DECLARE_H
#define COMP_DECLARE_H
#include "defs.h"
#include "fault.h"
#include "mem/scope.h"
#include "mem/symbols.h"
#include "util/arena.h"

enum { NDEV = 256, NLETTERS = 26 };

/* A fixed array field's dimension, ours: its lower bound, its elements. */
typedef struct {
    int32_t lb;
    uint32_t count;
} FieldDim;

/* A field of a TYPE. */
typedef struct {
    uint16_t name;
    uint8_t et, sub; /* its ET_*, or its TYPE's; an array's, its element's */
    uint16_t fix, off;
    uint8_t ndim; /* ours: a fixed array's dimensions, 0 none */
    uint16_t dim; /* its first in Decl.dims */
} Field;

/* A TYPE: its name, its fields, its bytes as QBasic lays them; ours, its STRINGs' Desc offsets,
   its records' and arrays' too. */
typedef struct {
    uint16_t name, nfld, size;
    uint32_t cap;
    Field *fields;
    uint32_t nstr, strcap;
    uint16_t *strs;
} TypeDef;

/* A DEFtype line: the DEFtypes from it on. */
typedef struct {
    uint32_t line;
    uint8_t deft[NLETTERS];
} DefEvent;

/* The program's declarations; its arena holds the tables. */
typedef struct {
    Arena arena;
    Fault *fault;
    uint16_t proc;           /* lx.dproc: the procedure being entered, its name; 0 the module */
    uint16_t ntypes;         /* lx.ntyps: the last TYPE's index */
    uint16_t nfields;        /* lx.nflds: all TYPEs' */
    uint16_t type;           /* lx.dtype: the TYPE being entered, 0 none */
    uint16_t scope;          /* g_dscope: where declarations go */
    bool redim;              /* g_dredim: the DIM is a REDIM */
    uint8_t base;            /* g_obase: OPTION BASE */
    bool baseset;            /* g_obset */
    bool dimmed;             /* g_odim: an array DIMmed (OPTION BASE after: an error) */
    bool explicit_;          /* g_oexp: OPTION EXPLICIT */
    bool deft;               /* decl_line's line has a DEFtype */
    uint32_t ndefev, defcap; /* g_ndev: DEFtype lines */
    DefEvent *defevs;
    uint32_t typecap;
    TypeDef **types; /* g_typ, from 1 */
    uint32_t ndims, dimcap;
    FieldDim *dims; /* the fixed array fields' dimensions */
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
/* Each DEFtype line's event, again from the program pg in its order: a line typed, or put
   between others, has its place. */
void decl_defevents(Decl *d, Scop *sc, Symb *sy, const Prog *pg);
/* dc_deft: the DEFtypes (sy->deft) line sees. */
void decl_deftype(const Decl *d, Symb *sy, uint32_t line);
/* TYPE t. */
static inline const TypeDef *decl_type(const Decl *const d, const uint16_t t) { return d->types[t]; }
/* The next statement from *at with no effect in 32-bit mode (DEF SEG, VARSEG, SSEG): its name,
   *at past it; null none. */
const char *decl_warning(const uint8_t **at);
const uint8_t *decl_item(const uint8_t *p); /* dc_item: past the item at p */
/* A TYPE's fixed array field's ( [lo TO] hi {, ..} ) at p, ours, each bound [-]n, base the lower
   one's when none: its dimensions to dims, MAXD at most, their count *n; past it, null not one. */
const uint8_t *decl_dims(const uint8_t *p, uint8_t base, FieldDim *dims, uint8_t *n);
/* Field f's dimension k. */
static inline const FieldDim *decl_fielddim(const Decl *const d, const Field *const f, const uint32_t k)
{
    return &d->dims[f->dim + k];
}
/* dc_proc: the body is a SUB's or FUNCTION's first line (PK_SUB, PK_FUNC; *name its M_VAR), their
   END (PK_END), else 0. */
uint8_t decl_proc(const uint8_t *body, const uint8_t **name);
/* The lines declared from here are procedure proc's (its name), 0 the module's. */
void decl_enter(Decl *d, uint16_t proc);
bool decl_tail(const uint8_t **at); /* *at the body to its last statement; true ends in a string,
                                       REM's or DATA's text */
const uint8_t *decl_step(const uint8_t *p); /* decl_item, a number kept as text whole */
#endif
