/* A module's exports as the other dialect sees them: its procedures and its records, in the VM's
 * types. How it works: export.c. */
#ifndef COMP_EXPORT_H
#define COMP_EXPORT_H
#include "defs.h"
#include "util/arena.h"

typedef struct Comp Comp;
typedef struct Binding Binding;
typedef struct Decl Decl;

/* Why a value has no type both dialects have: none, an array, a BYTE, a TYPE of one. */
typedef enum : uint8_t { EW_NONE, EW_ARRAY, EW_BYTE, EW_TYPE } ExpWhy;

/* A QBasic procedure's verdict, kept as declared (comp/declare.c) in its module's binding's val:
   its first parameter's why, its result's EXP_RESULT times over. */
enum { EXP_RESULT = 16 };

/* A parameter's or a result's type: ET_*, ET_REC + t a TYPE's, ET_IMP a SUB's result; byref a
   parameter by reference; a parameter's name as first written. */
typedef struct {
    uint32_t et;
    bool byref;
    const char *name;
    uint32_t len;
} ExpVal;

/* A procedure: its name as first written, no suffix; its number (0 till its front end links it);
   its parameters, a record result's last when resarg; why it can't be called across, null when
   it can. */
typedef struct {
    const char *name;
    uint32_t len;
    uint32_t proc;
    ExpVal result;
    ExpVal *params;
    uint32_t npar;
    bool resarg;
    const char *why;
} ExpProc;

/* A record's field: its name, its type (an ExpVal's), its bytes into the record. */
typedef struct {
    const char *name;
    uint32_t len;
    uint32_t et;
    uint32_t off;
} ExpField;

/* A record: its name, its TYPE (comp/declare.h), its fields and bytes, whether it holds strings;
   why it can't be used across, null when it can. */
typedef struct {
    const char *name;
    uint32_t len;
    uint32_t td;
    ExpField *fields;
    uint32_t nfields;
    uint32_t size;
    bool deep;
    const char *why;
} ExpType;

typedef struct Exports {
    ExpProc *procs;
    uint32_t nprocs;
    ExpType *types;
    uint32_t ntypes;
} Exports;

/* QBasic module m's SUBs, FUNCTIONs and TYPEs but the PRIVATE ones, after link_prepare: each
   value's type one both dialects have (ET_I2, ET_I4, ET_SH, ET_R4, ET_R8, ET_SD, a TYPE of them),
   else why not. In a's room; Out of memory through C's fault. */
Exports expo_qbasic(const Comp *C, uint32_t m, Arena *a);
/* Quickr module m's exports x, declared as QBasic's in module m: each procedure a SUB, or a
   FUNCTION of its result's type; each record usable a TYPE. The first name two of them spell,
   case aside; 0 none. */
uint32_t expo_declare(Comp *C, uint32_t m, const Exports *x);
/* Why the value bound by b (its array's when arr) has no type both dialects have. */
ExpWhy expo_value_why(const Decl *d, const Binding *b, bool arr);
/* Can the other dialect use name t, a module's export: not a procedure or a TYPE whose verdict,
   declared, says it can't? */
bool expo_usable(const Comp *C, uint32_t t);
#endif
