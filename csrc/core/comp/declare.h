/* A line's declarations, into the scopes as it is entered (comp/decl.asm), so lines compiled before
   it know them, as QBasic: DEFtype, DIM [SHARED] .. AS, CONST, SUB, FUNCTION, DECLARE, DEF FN and
   their parameters, SHARED, STATIC, COMMON, TYPE's fields, OPTION BASE and EXPLICIT; n( an array
   known in scope from there on. Each goes into the binding of decl.scope: the module's or the
   procedure being entered's. DEFtype holds from its line to the file's end: each loading line's
   is an event, decl_deftype sets the DEFtypes for the line compiled. */
#ifndef COMP_DECLARE_H
#define COMP_DECLARE_H
#include "defs.h"

/* A TYPE: its name, its first field, how many, its bytes as QBasic lays them. */
typedef struct PACKED {
    uint16_t name, first, nfld, size;
} TypeDef;
static_assert(sizeof(TypeDef) == 8);

/* A TYPE's field: its name, ET_*, its TYPE (a record's), STRING * n, its offset. */
typedef struct PACKED {
    uint16_t name;
    uint8_t et, sub;
    uint16_t fix, off;
} Field;
static_assert(sizeof(Field) == 8);

enum { NDEV = 256, NLETTERS = 26 };
/* A DEFtype line: the DEFtypes from it on. */
typedef struct {
    uint32_t line;
    uint8_t deft[NLETTERS];
} DefEvent;

typedef struct {
    uint16_t proc;    /* lx.dproc: the procedure being entered, its name; 0 the module */
    uint16_t ntypes;  /* lx.ntyps: the last TYPE's index */
    uint16_t nfields; /* lx.nflds */
    uint16_t type;    /* lx.dtype: the TYPE being entered, 0 none */
    uint16_t scope;   /* g_dscope: where declarations go */
    bool redim;       /* g_dredim: the DIM is a REDIM */
    uint8_t base;     /* g_obase: OPTION BASE */
    bool baseset;     /* g_obset */
    bool dimmed;      /* g_odim: an array DIMmed (OPTION BASE after: an error) */
    bool explicit_;   /* g_oexp: OPTION EXPLICIT */
    uint32_t ndefev;  /* g_ndev: DEFtype lines */
    DefEvent defevs[NDEV];
    const uint8_t *at;    /* (ESI) the body's cursor */
    TypeDef types[NTYPE]; /* g_typ, from 1 */
    Field fields[NFLD];   /* g_fld */
} Decl;

extern const uint8_t decl_astype[5]; /* g_dast: AS INTEGER .. AS STRING to ET_* */
extern const uint8_t decl_etsize[7]; /* g_etsz: a field's bytes by ET_* */

void decl_reset(Session *S);                     /* no declarations; the names kept */
void decl_line(Session *S, const uint8_t *body); /* enter the body's declarations */
void decl_deftype(
    Session *S,
    uint32_t line
);                                          /* dc_deft: the DEFtypes (mem/sym's g_deft) line sees */
const uint8_t *decl_item(const uint8_t *p); /* dc_item: past the item at p */
/* dc_proc: the body is a SUB's or FUNCTION's first line (PK_SUB, PK_FUNC; *name its M_VAR), their
   END (PK_END), else 0. */
uint8_t decl_proc(const uint8_t *body, const uint8_t **name);
bool decl_tail(const uint8_t **at); /* *at the body to its last statement; true ends in a string,
                                       REM's or DATA's text */
const uint8_t *decl_step(const uint8_t *p); /* decl_item, a number kept as text whole */
#endif
