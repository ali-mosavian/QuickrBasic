/* A stopped run's variables and values, as a debugger sees them. How it works: inspect.c. */
#ifndef VM_INSPECT_H
#define VM_INSPECT_H
#include "session.h"

/* A stopped run's frame is its number, 0 the newest; a debugger keeps no more of it. */

/* A variable a debugger lists: its name's id (the program's tokens'), the suffix its type shows (0
   none), whether it is a procedure's own (else a module's); its id is this module's. */
typedef struct {
    uint32_t id, name;
    char sfx;
    bool local;
} InspVar;

/* A value of the stopped run: a number or string, a record, an array; none (a record not made).
   A debugger reads it only through insp_kind and the functions below. */
typedef enum : uint8_t { IV_NONE, IV_SCALAR, IV_RECORD, IV_ARRAY } InspKind;
typedef struct {
    InspKind kind;
    uint16_t fix;  /* STRING * n's n */
    uint32_t type; /* ET_*, ET_REC + t */
    Addr at;       /* a scalar's or a record's place; an array's header, or its field's place */
    uint32_t key;  /* a fixed array field's (decl_fieldkey); 0 a header's array */
} InspValue;

/* The stopped run's frames; frame f's program line, DLINE the immediate window's. */
uint32_t insp_frames(const Session *S);
uint32_t insp_line(const Session *S, uint32_t f);
/* Frame f is its procedure's newest call: its variables are its own, not saved by a newer one. */
bool insp_newest(const Session *S, uint32_t f);
/* Frame f's procedure's parameters, none at the module. To out, max at most; how many. */
uint32_t insp_params(const Session *S, uint32_t f, InspVar *out, uint32_t max);
/* Frame f's procedure's variables (the module's at the module): its parameters first, then its own
   by id; DEF FNs' and hidden ones left out. To out, max at most; how many. */
uint32_t insp_vars(const Session *S, uint32_t f, InspVar *out, uint32_t max);
/* The module's variables frame f's procedure sees, DIM SHARED or SHARED in it, by id; none at the
   module. */
uint32_t insp_shared(const Session *S, uint32_t f, InspVar *out, uint32_t max);
/* Variable var as frame f sees it. */
InspValue insp_var(const Session *S, uint32_t f, const InspVar *var);
/* PRINT expr in frame f's procedure's scope, its output to out (session_eval): its error, else
   ST_OK. */
Status insp_eval(Session *S, uint32_t f, const char *expr, char out[static 80]);
/* Frame f's variables are the ones a statement run in its procedure's scope changes: the stopped
   call's, or the module's; no direct line called into the run. */
bool insp_settable(const Session *S, uint32_t f);
/* path = value run in frame f's procedure's scope (session_exec), f insp_settable: its error, else
   ST_OK. */
Status insp_set(Session *S, uint32_t f, const char *path, const char *value);
static inline InspKind insp_kind(const InspValue *const v) { return v->kind; }
static inline bool insp_string(const InspValue *const v)
{
    return v->kind == IV_SCALAR && v->type == ET_SD;
}
/* Name name's own text (capitals) at d; past it. */
char *insp_name(const Session *S, char *d, uint32_t name);
/* Variable var is the name s, n long, suffix sfx (0 none): its letters the name's, the suffix, if
   any, its type's. */
bool insp_is(const Session *S, const InspVar *var, const uint8_t *s, uint32_t n, uint8_t sfx);
/* Scalar v as text at d, room characters of a string's at most: a number as PRINT's without its
   sign's space, a string in quotes, its 0s spaces. Past it. */
char *insp_text(const Session *S, const InspValue *v, char *d, uint32_t room);
/* String v's length; its first room characters at d, its 0s spaces. */
uint32_t insp_chars(const Session *S, const InspValue *v, char *d, uint32_t room);
/* Record v's TYPE's name id; its fields; field i's value, its name id to *name. */
uint32_t insp_type(const Session *S, const InspValue *v);
uint32_t insp_nfields(const Session *S, const InspValue *v);
InspValue insp_field(const Session *S, const InspValue *v, uint32_t i, uint32_t *name);
/* Array v's dimensions, 0 not made; dimension i's lower bound, its count; its elements; element i,
   the first subscript the fastest as the run lays them. */
uint8_t insp_dims(const Session *S, const InspValue *v);
int32_t insp_lbound(const Session *S, const InspValue *v, uint8_t i);
uint32_t insp_count(const Session *S, const InspValue *v, uint8_t i);
uint32_t insp_elems(const Session *S, const InspValue *v);
InspValue insp_elem(const Session *S, const InspValue *v, uint32_t i);
#endif
