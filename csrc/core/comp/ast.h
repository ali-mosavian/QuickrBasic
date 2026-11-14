/* An expression's tree: scratch, built by the parser (expr.c), folded (fold.c), turned into code
   and dropped within one expr_compile. A nested expression (a function's argument) builds its
   own above it: nodes and leaf code are stacks, released when that expression's code is made.

   Leaves: a literal load (N_LIT), a scalar variable (N_VAR), or code compiled where the parser met
   it (N_CODE: a function call, an array element, a record's field, a string literal). */
#ifndef COMP_AST_H
#define COMP_AST_H
#include "defs.h"

typedef enum : uint8_t { N_LIT, N_VAR, N_CODE, N_UN, N_BIN } NodeKind;

enum { AST_NODES = 1024, AST_CODE = 0x4000, LIT_MAX = 8 };

typedef struct {
    NodeKind kind;
    uint8_t type;      /* its value's T_* */
    uint8_t sh;        /* its SHORT class, SH_* */
    uint8_t op;        /* N_LIT: its load op; N_UN: TK_MINUS or TK_NOT; N_BIN: its operator */
    bool dbl;          /* N_BIN: parsed for a DOUBLE target (expr.dbltgt) */
    uint16_t a, b;     /* N_UN, N_BIN: the operands; N_VAR: the variable */
    uint16_t at, len;  /* N_CODE: its code in Ast.code */
    uint8_t lit[LIT_MAX]; /* N_LIT: its operand bytes */
    const uint8_t *end; /* N_UN, N_BIN: the token past its last operand: an error's place */
} Node;

typedef struct {
    Node node[AST_NODES];
    uint16_t nnodes;
    uint8_t code[AST_CODE];
    uint16_t ncode;
} Ast;
#endif
