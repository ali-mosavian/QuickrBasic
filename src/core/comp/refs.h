/* Reference parsing's interface. How it works: refs.c. */
#ifndef COMP_REFS_H
#define COMP_REFS_H
#include "comp/ast.h"
#include "comp/blocks.h"
#include "comp/comp.h"

/* How a call's arguments are written: ( args ) if there, args to the statement's end, none. */
typedef enum : uint8_t { ARGS_PAREN, ARGS_BARE, ARGS_NONE } ArgsForm;

Node *ref_record(
    Comp *C
); /* rc_ref: a record reference at comp.p, its N_REF; nullptr none, comp.p kept */
Node *ref_field(
    Comp *C,
    Node *ref
); /* rc_load: ref's field's value; Type mismatch a whole record */
/* VARPTR$'s type byte of place at, QBasic's: 2 INTEGER, 20 LONG, 4 SINGLE, 8 DOUBLE, 3 STRING.
   Type mismatch: a record. */
uint8_t ref_vtype(Comp *C, const Node *at);
Node *ref_place(
    Comp *C,
    uint32_t *vakind
); /* cm_addr: a variable's place at comp.p, its VA_* kind */
/* ref_place's, for GET, PUT and FIELD: Type mismatch a record holding a STRING or an array. */
Node *ref_bytes(Comp *C, uint32_t *vakind);
bool ref_len(
    Comp *C,
    uint32_t *size
); /* LEN(r) at comp.p: a record's, a field's, a number's size */
uint32_t ref_recarray(
    Comp *C,
    uint32_t name,
    uint32_t type
);                                           /* rc_avar: record array name's variable */
const Proc *ref_proc(Comp *C, uint32_t *idx); /* ct_proc: the procedure named at comp.p; null none */
uint32_t ref_arrayof(Comp *C, uint32_t id);  /* array id, or its name's record array */
/* ours: a call at comp.p of a FUNCTION whose result is a record or an array, its N_CALL, a the place
   of what the result went to; null none: in that FUNCTION its name alone, and an array result's
   name(..), are its result. */
Node *ref_result(Comp *C);
uint32_t ref_wholearray(Comp *C); /* a() at comp.p, stepped past: its variable */
Node *ref_cell(
    Comp *C,
    uint32_t id
); /* variable id's cell, an argument's ref: a parameter's is its argument's */
/* A record's dynamic array field whole at comp.p, ours, f or f(): its cell's place, *type its
   elements' (a variable's ET_*); null, comp.p kept: none. */
Node *ref_fieldarray(Comp *C, uint32_t *type);
/* An array at comp.p, ours: a() whole, or f(..), a FUNCTION's array result, its call to *call (null
   for a()): its variable, a record for a record result. */
uint32_t ref_arrayvalue(Comp *C, Node **call);
Node *ref_recplace(Comp *C, uint32_t id); /* record variable id's place, the whole of it */
Node *ref_call(
    Comp *C,
    const uint8_t *name,
    uint32_t idx,
    const Proc *pr,
    ArgsForm form
);                        /* its N_CALL */
Node *ref_undef(Comp *C); /* FUNCTION DECLAREd only at comp.p: Function not defined; or N_CALL */
void ref_dropargs(
    Comp *C,
    bool paren
); /* ct_uarg: a DECLAREd-only call's arguments checked, dropped */
#endif
