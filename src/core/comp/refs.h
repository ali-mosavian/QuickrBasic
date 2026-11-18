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
bool ref_len(
    Comp *C,
    uint32_t *size
); /* LEN(r) at comp.p: a record's, a field's, a number's size */
uint16_t ref_recarray(
    Comp *C,
    uint16_t name,
    uint8_t type
);                                           /* rc_avar: record array name's variable */
const Proc *ref_proc(Comp *C, uint8_t *idx); /* ct_proc: the procedure named at comp.p; null none */
Node *ref_call(Comp *C, const uint8_t *name, uint8_t idx, const Proc *pr, ArgsForm form); /* its N_CALL */
Node *ref_undef(Comp *C); /* FUNCTION DECLAREd only at comp.p: Function not defined; or N_CALL */
void ref_dropargs(
    Comp *C,
    bool paren
); /* ct_uarg: a DECLAREd-only call's arguments checked, dropped */
#endif
