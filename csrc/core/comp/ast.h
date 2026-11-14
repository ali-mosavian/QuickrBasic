/* An expression's tree: scratch, built by the parser (expr.c), folded (fold.c), turned into code
   and dropped within one expr_compile. A nested expression (a function's argument) builds its
   own above it: nodes and leaf code are stacks, released when that expression's code is made.

   Leaves: a literal load (N_LIT: a number, a string, a CONST's value), a scalar variable (N_VAR),
   or code compiled where the parser met it (N_CODE: a FUNCTION's call, a record's field, VARPTR,
   LEN of a record). An element (N_ELEM) and QBasic's functions (N_FN: SGN .. MID$; N_XFN: the
   others) have their arguments as children, linked by next. */
#ifndef COMP_AST_H
#define COMP_AST_H
#include "defs.h"

typedef enum : uint8_t { N_LIT, N_VAR, N_CODE, N_UN, N_BIN, N_ELEM, N_FN, N_XFN } NodeKind;

enum { AST_NODES = 1024, AST_CODE = 0x4000, LIT_MAX = 8, NO_NODE = 0xFFFF };

/* N_XFN's mode: its arguments only, their count after them, or none given: a 0. */
enum : uint8_t { XM_ARGS, XM_COUNT, XM_NONE };

typedef struct {
    NodeKind kind;
    uint8_t type;      /* its value's T_* */
    uint8_t sh;        /* its SHORT class, SH_* */
    uint8_t op;        /* N_LIT: its load op; N_UN: TK_MINUS or TK_NOT; N_BIN: its operator;
                          N_FN: the function, TK_SGN on; N_XFN: its xfns entry */
    bool dbl;          /* N_BIN, N_FN: parsed for a DOUBLE target (expr.dbltgt) */
    uint8_t conv;      /* an N_XFN's argument: its kind (I F D S W G X, R an array's id) */
    uint8_t mode;      /* N_XFN: XM_* */
    uint8_t nargs;     /* N_ELEM, N_FN, N_XFN: the arguments */
    uint16_t a, b;     /* N_UN, N_BIN: the operands; N_VAR, N_ELEM: the variable; b: the first
                          argument */
    uint16_t next;     /* an argument: the next, NO_NODE none */
    uint16_t at, len;  /* N_CODE: its code in Ast.code */
    uint8_t lit[LIT_MAX]; /* N_LIT: its operand bytes */
    const uint8_t *end; /* N_UN, N_BIN, N_FN, N_XFN: past its last operand, an error's place */
} Node;

typedef struct {
    Node node[AST_NODES];
    uint16_t nnodes;
    uint8_t code[AST_CODE];
    uint16_t ncode;
} Ast;
#endif
