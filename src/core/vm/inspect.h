/* A stopped run's variables and values, as a debugger sees them. How it works: inspect.c. */
#ifndef VM_INSPECT_H
#define VM_INSPECT_H
#include "session.h"

/* A variable a debugger lists: its id, its name's id, the suffix its type shows (0 none), whether
   it is a procedure's own (else a module's). */
typedef struct {
    uint32_t id, name;
    char sfx;
    bool local;
} InspVar;

/* A value of the stopped run: a number or string, a record, an array; none (an array not made).
   Its other fields are this module's: a debugger only passes it back. */
typedef enum : uint8_t { IV_NONE, IV_SCALAR, IV_RECORD, IV_ARRAY } InspKind;
typedef struct {
    InspKind kind;
    uint16_t fix;  /* STRING * n's n */
    uint32_t type; /* ET_*, ET_REC + t */
    Addr at;       /* a scalar's or a record's place; an array's header, or its field's place */
    uint32_t key;  /* a fixed array field's (decl_fieldkey); 0 a header's array */
} InspValue;

/* Procedure proc's variables (0 the module's): its parameters first, then its own by id; DEF FNs'
   and hidden ones left out. To out, max at most; how many. */
uint32_t insp_vars(const Session *S, uint32_t proc, InspVar *out, uint32_t max);
/* The module's variables procedure proc sees, DIM SHARED or SHARED in it, by id. */
uint32_t insp_shared(const Session *S, uint32_t proc, InspVar *out, uint32_t max);
/* Variable var as frame f sees it. */
InspValue insp_var(const Session *S, const DebugFrame *f, const InspVar *var);
/* Name name's own text (capitals) at d; past it. */
char *insp_name(const Session *S, char *d, uint32_t name);
/* Variable var is the name s, n long, suffix sfx (0 none): its letters the name's, the suffix, if
   any, its type's. */
bool insp_is(const Session *S, const InspVar *var, const uint8_t *s, uint32_t n, uint8_t sfx);
/* Scalar v as text at d, room characters of a string's at most: a number as PRINT's without its
   sign's space, a string in quotes. Past it. */
char *insp_text(const Session *S, const InspValue *v, char *d, uint32_t room);
#endif
