/* The expression tree's interface. How it works: ast.c. */
#ifndef COMP_AST_H
#define COMP_AST_H
#include "base.h"
#include "defs.h"
#include "fault.h"
#include "util/arena.h"
#include "mem/symbols.h"
#include "vm/exec.h"

typedef enum : uint8_t {
    N_LIT,
    N_VAR,
    N_UN,
    N_BIN,
    N_ELEM,
    N_FN,
    N_XFN,
    N_REF,
    N_FIELD,
    N_ADDR,
    N_CALL,
    N_ARG,
    N_KCALL,
    N_CONV,
} NodeKind;

enum { AST_CHUNK = 0x4000, LIT_MAX = 8 };

/* N_CONV's conversion of its child: to an INT, a FLOAT, a DOUBLE, a string, a byte, an address. */
enum : uint8_t { CV_INT, CV_FLT, CV_DBL, CV_STR, CV_BYTE, CV_ADDR };

/* An operator's index: + - * / ^ AND OR > = < from TK_PLUS, then \ MOD XOR EQV IMP. A relation
   compiles as REL and its DC_* mask. */
enum : uint8_t {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_POW,
    OP_AND,
    OP_OR,
    OP_GT,
    OP_EQ,
    OP_LT,
    OP_IDIV,
    OP_MOD,
    OP_XOR,
    OP_EQV,
    OP_IMP,
    NOPS,
};
/* A relation's operator byte: REL and its DC_* mask. */
enum : uint8_t { REL = 0x80, REL_MASK = 7 };
static inline bool ast_isrel(const uint8_t op) { return op & REL; }
static inline bool ast_islogic(const uint8_t op) { return op == OP_AND || op == OP_OR; }
static inline bool ast_isxop(const uint8_t op)
{
    return op >= OP_IDIV && op < REL;
} /* \ MOD XOR EQV IMP */

/* A SHORT's class: a value of no SHORT, a SHORT's, a literal that fits one. */
enum : uint8_t { SH_NONE = 0, SH_SHORT = 1, SH_LIT = 2 };

/* How a binary node computes: its operands' type. */
enum : uint8_t { W_INT, W_FLT, W_DBL, W_POW, W_STR };

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

typedef struct Node Node;
struct Node {
    NodeKind kind;
    uint8_t type;  /* its value's T_* */
    uint8_t sh;    /* its SHORT class, SH_* */
    uint8_t op;    /* N_LIT: its load op; N_UN: TK_MINUS or TK_NOT; N_BIN: its operator;
                          N_FN: the function, TK_SGN on; N_XFN: its xfns entry; N_REF: O_R_ADDR,
                          O_RA_ADDR, O_V_ADDR; N_ADDR: its CALL, 0 none; N_ARG: PA_*;
                          N_CALL: the procedure's index; N_KCALL: its K_*; N_CONV: CV_* */
    bool dbl;      /* N_BIN, N_FN: parsed for a DOUBLE target (expr.dbltgt) */
    uint8_t conv;  /* an N_XFN's argument: its kind (I F D S W G X, R an array's id) */
    uint8_t mode;  /* N_XFN: XM_*; N_CALL: CALL_* */
    uint8_t nargs; /* N_ELEM, N_FN, N_XFN, N_REF, N_CALL: the arguments */
    Node *a, *b;   /* N_UN, N_BIN: the operands; N_FIELD, N_ADDR, N_ARG, N_CONV: a the child;
                          b: the first argument */
    Node *next;    /* an argument, a step: the next, null none */
    uint16_t id;   /* N_VAR, N_ELEM, N_REF: the variable; N_ARG: PA_VALUE's parameter, an array's
                          or a record's; N_CALL CALL_UNDEF: its result */
    RecRef rec;    /* N_REF of a record: the place in it */
    /* Resolved by sema (comp/sema.h), for gen: */
    const Var *var;       /* id's variable */
    bool par;             /* id a scalar parameter of the procedure compiled */
    Jump jump;            /* N_CALL CALL_PROC: its procedure's entry */
    uint8_t lit[LIT_MAX]; /* N_LIT: its operand bytes */
    const uint8_t
        *end; /* N_UN, N_BIN, N_FN, N_XFN, N_CONV: past its last operand, an error's place */
};

/* The statement's arena; its steps, first to last. */
typedef struct {
    Arena arena;
    Fault *fault;
    Node *steps, *last;
} Ast;

/* Out of memory raises through f. */
void ast_init(Ast *t, Fault *f);
/* A statement starts: every node freed. */
void ast_reset(Ast *t);
/* A node, zeroed but for kind; Out of memory when the arena is full. */
Node *ast_new(Ast *t, NodeKind kind);
void ast_link(Node *parent, Node **last, Node *arg); /* arg after *last, parent's first if null */
bool ast_islit(const Node *n);
uint8_t ast_type(const Node *n); /* n's value's T_*, nothing compiled */
uint8_t ast_work(const Node *n); /* binary node n's W_* */
/* The statement's next step: tree n. */
void ast_step(Ast *t, Node *n);
/* The last step converted, CV_*; end its place for an error. */
void ast_conv(Ast *t, uint8_t cv, const uint8_t *end);
#endif
