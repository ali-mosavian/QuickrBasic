/* References parsed into the tree (comp/ast.h): a record's field or all of a record, an element's
   or a variable's place, a procedure's call and its arguments. Names are resolved and nodes built
   here; nothing is emitted (expr.c's gen does). */
#ifndef COMP_REF_H
#define COMP_REF_H
#include "comp/ast.h"
#include "comp/blok.h"
#include "comp/comp.h"

/* How a call's arguments are written: ( args ) if there, args to the statement's end, none. */
typedef enum : uint8_t { ARGS_PAREN, ARGS_BARE, ARGS_NONE } ArgsForm;

Node *ref_record(Session *S); /* rc_ref: a record reference at comp.p, its N_REF; nullptr none, comp.p kept */
Node *ref_field(Session *S, Node *ref); /* rc_load: ref's field's value; Type mismatch a whole record */
Node *ref_place(Session *S, uint32_t *vakind); /* cm_addr: a variable's place at comp.p, its VA_* kind */
bool ref_len(Session *S, uint32_t *size); /* LEN(r) at comp.p: a record's, a field's, a number's size */
uint16_t ref_recarray(Session *S, uint16_t name, uint8_t type); /* rc_avar: record array name's variable */
const Proc *ref_proc(Session *S, uint8_t *idx); /* ct_proc: the procedure named at comp.p; null none */
Node *ref_call(Session *S, uint8_t idx, const Proc *pr, ArgsForm form); /* its N_CALL */
Node *ref_undef(Session *S); /* a FUNCTION DECLAREd only, at comp.p: its N_CALL CALL_UNDEF */
void ref_dropargs(Session *S, bool paren); /* ct_uarg: a DECLAREd-only call's arguments checked, dropped */
#endif
