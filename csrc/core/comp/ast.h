/* An expression's tree: scratch, built by the parser (expr.c, ref.c), folded (fold.c), turned into
   code (expr.c's gen) and dropped within one expression. A nested tree builds above the one
   that holds it: nodes are a stack, released when that tree's code is made.

   Leaves: a literal load (N_LIT: a number, a string, a CONST's value, a size), a scalar variable
   (N_VAR). An element (N_ELEM), QBasic's functions (N_FN: SGN .. MID$; N_XFN: the others), a
   place (N_REF) and a procedure's call (N_CALL) have their arguments as children, linked by
   next. A record's field (N_FIELD), an address (N_ADDR) and a call's argument (N_ARG) have one
   child, a. */
#ifndef COMP_AST_H
#define COMP_AST_H
#include "base.h"
#include "defs.h"

typedef enum : uint8_t {
    N_LIT, N_VAR, N_UN, N_BIN, N_ELEM, N_FN, N_XFN, N_REF, N_FIELD, N_ADDR, N_CALL, N_ARG
} NodeKind;

enum { AST_NODES = 1024, LIT_MAX = 8, NO_NODE = 0xFFFF };

/* N_XFN's mode: its arguments only, their count after them, or none given: a 0. */
enum : uint8_t { XM_ARGS, XM_COUNT, XM_NONE };

/* N_ARG's passing: the value and FFFFh; a variable's cell (0, V.ADDR); an element's or a field's
   P.REF; an array whole (its id twice); a record whole (its id, FFFFh). */
enum : uint8_t { PA_VALUE, PA_VAR, PA_PREF, PA_ARRAY, PA_RECORD };

/* N_CALL's mode: a procedure's, or one DECLAREd only: ERROR when run, then its result's load. */
enum : uint8_t { CALL_PROC, CALL_UNDEF };

/* A record's place: its field's ET_* (ET_REC + t all of a record of TYPE t), offset, its STRING * n
   or a record's size. */
typedef struct {
    uint8_t et;
    uint16_t off;
    uint16_t fix;
} RecRef;

typedef struct {
    NodeKind kind;
    uint8_t type;      /* its value's T_* */
    uint8_t sh;        /* its SHORT class, SH_* */
    uint8_t op;        /* N_LIT: its load op; N_UN: TK_MINUS or TK_NOT; N_BIN: its operator;
                          N_FN: the function, TK_SGN on; N_XFN: its xfns entry; N_REF: O_R_ADDR,
                          O_RA_ADDR, O_V_ADDR; N_ADDR: its CALL, 0 none; N_ARG: PA_*;
                          N_CALL: the procedure's index */
    bool dbl;          /* N_BIN, N_FN: parsed for a DOUBLE target (expr.dbltgt) */
    uint8_t conv;      /* an N_XFN's argument: its kind (I F D S W G X, R an array's id) */
    uint8_t mode;      /* N_XFN: XM_*; N_CALL: CALL_* */
    uint8_t nargs;     /* N_ELEM, N_FN, N_XFN, N_REF, N_CALL: the arguments */
    uint16_t a, b;     /* N_UN, N_BIN: the operands; N_FIELD, N_ADDR, N_ARG: a the child;
                          b: the first argument */
    uint16_t next;     /* an argument: the next, NO_NODE none */
    uint16_t id;       /* N_VAR, N_ELEM, N_REF: the variable; N_ARG: PA_VALUE's parameter, an array's
                          or a record's; N_CALL CALL_UNDEF: its result */
    RecRef rec;        /* N_REF of a record: the place in it */
    uint8_t lit[LIT_MAX]; /* N_LIT: its operand bytes */
    const uint8_t *end; /* N_UN, N_BIN, N_FN, N_XFN: past its last operand, an error's place */
} Node;

typedef struct {
    Node node[AST_NODES];
    uint16_t nnodes;
} Ast;

uint16_t ast_new(Session *S, NodeKind kind); /* a node, zeroed but for kind; Out of memory full */
Node *ast_node(Session *S, uint16_t i);
void ast_link(Session *S, uint16_t parent, uint16_t *last, uint16_t arg); /* arg after *last */
#endif
