/* Quickr's front end's records: the source's lines and tokens, the tree, its names and the errors.
 * How it works: qast.c. */
#ifndef QR_QAST_H
#define QR_QAST_H
#include "base.h"
#include "fault.h"
#include "util/arena.h"

/* The front end's scratch: Out of memory raises through fault. */
typedef struct {
    Arena arena;
    Fault *fault;
} QMem;

/* ---- the source */

/* A source line: its record in the program store, its text (the arena's copy), its number. */
typedef struct {
    uint32_t at;
    const char *text;
    uint32_t len;
    uint16_t src;
} QLine;

typedef enum : uint8_t {
    QK_EOF,
    QK_NL, /* a statement's end: a line's, outside brackets */
    QK_NAME,
    QK_INT,
    QK_REAL,
    QK_STR,
    /* keywords, in qlex.c's table's order */
    QK_AND,
    QK_AS,
    QK_BREAK,
    QK_CONST,
    QK_CONTINUE,
    QK_DIV,
    QK_DO,
    QK_ELSE,
    QK_ELSEIF,
    QK_END,
    QK_FALSE,
    QK_FN,
    QK_FOR,
    QK_IF,
    QK_IN,
    QK_LET,
    QK_MOD,
    QK_MUT,
    QK_NOT,
    QK_OR,
    QK_RETURN,
    QK_STEP,
    QK_THEN,
    QK_TRUE,
    QK_VAR,
    QK_WHILE,
    QK_XOR,
    QK_LATER, /* a keyword of a later milestone: type, match, import .. */
    /* punctuation */
    QK_LPAREN,
    QK_RPAREN,
    QK_LBRACK,
    QK_RBRACK,
    QK_LBRACE,
    QK_RBRACE,
    QK_COMMA,
    QK_COLON,
    QK_DOT,
    QK_DOTDOT,
    QK_QUEST,
    QK_BANG,
    QK_PLUS,
    QK_MINUS,
    QK_STAR,
    QK_SLASH,
    QK_POW,
    QK_EQ,
    QK_NE,
    QK_LT,
    QK_LE,
    QK_GT,
    QK_GE,
    QK_AMP,
    QK_BAR,
    QK_CARET,
    QK_TILDE,
    QK_SHL,
    QK_SHR,
    QK_ADDEQ,
    QK_SUBEQ,
    QK_MULEQ,
    QK_DIVEQ,
    QK_BAD, /* the lexer reported it */
} QKind;

/* A token: its line's index, its column from 0, its text in that line. A number's value; a
   string's text is between its quotes, escapes as written. */
typedef struct {
    QKind kind;
    uint32_t line;
    uint32_t col;
    uint32_t len;
    const char *text;
    int64_t i;
    double d;
} QTok;

/* The source: its lines, their tokens, the last QK_EOF. */
typedef struct {
    QLine *lines;
    uint32_t nlines;
    QTok *toks;
    uint32_t ntoks;
} QSrc;

/* ---- errors */

typedef struct {
    uint32_t line; /* its line's index */
    uint32_t col;
    const char *text;
    uint32_t len;
} QErr;

typedef struct {
    QMem *mem;
    QErr *at;
    uint32_t n, cap;
} QErrs;

/* An error at token t: msg with its {} (if any) replaced by name's len bytes. */
void qast_error(QErrs *e, const QTok *t, const char *msg, const char *name, uint32_t len);
/* The errors in order of line and column. */
void qast_sort(QErrs *e);

/* ---- types and values */

typedef enum : uint8_t {
    QY_VOID,
    QY_INT,
    QY_SINGLE,
    QY_DOUBLE,
    QY_STRING,
    QY_BOOL,
    QY_ERROR, /* an error's: no further error about it */
} QType;

/* A string constant's part: a literal's token. */
typedef struct QPiece QPiece;
struct QPiece {
    const QTok *lit;
    const QPiece *next;
};

/* A constant: an int's or a bool's (0, 1) in i, a single's or a double's in d, a string's parts. */
typedef struct {
    int64_t i;
    double d;
    const QPiece *s;
} QVal;

typedef struct QVar QVar;
typedef struct QFn QFn;
typedef struct QExpr QExpr;
typedef struct QStmt QStmt;

/* ---- names */

typedef enum : uint8_t {
    QV_LET,
    QV_VAR,
    QV_CONST,
    QV_PARAM,  /* read-only, by address */
    QV_MUT,    /* a mut parameter */
    QV_LOOP,   /* a for's counter */
    QV_TEMP,   /* the compiler's: a chain's middle, a for's limit and step */
    QV_RESULT, /* a fn's result */
} QVarKind;

struct QVar {
    const QTok *name; /* its declaration's; a temporary's: its expression's first token */
    QType type;
    QVarKind kind;
    QFn *fn;       /* its fn, null the module's */
    QVal val;      /* a const's */
    bool constant; /* a const: val, untyped as its value's */
    bool untyped;
    uint32_t id; /* its variable (mem/symbols.h), qlink's */
    QVar *next;  /* the program's next */
};

typedef struct {
    const QTok *name;
    const QTok *type; /* its as type's name, null none */
    QExpr *def;       /* its default, folded; null none */
    bool mut;
    QVar *var;
} QParam;

struct QFn {
    const QTok *name;
    QParam *params;
    uint32_t npar;
    const QTok *rtype; /* its as type's name, null none */
    QType result;
    bool sig;  /* its signature resolved */
    QVar *res; /* its result's variable, null none */
    QStmt *def, *end;
    uint32_t proc; /* its procedure (comp/blocks.h), qlink's */
    QFn *next;     /* the program's next */
};

/* ---- expressions */

typedef enum : uint8_t {
    QE_CONST,  /* val */
    QE_NAME,   /* tok; var once resolved */
    QE_UNARY,  /* op a */
    QE_BINARY, /* a op b */
    QE_CHAIN,  /* items[0] ops[0] items[1] ..: comparisons, each pair's type works[k] */
    QE_AND,
    QE_OR,
    QE_COND, /* a if c else b */
    QE_IN,   /* a in b..c, not in when neg; work the three's type */
    QE_CALL, /* tok's fn or built-in with args; argv by parameter once resolved */
    QE_CONV, /* a converted to type */
} QExprKind;

/* A built-in: print, a conversion. */
typedef enum : uint8_t { QB_NONE, QB_PRINT, QB_INT, QB_SINGLE, QB_DOUBLE, QB_STRING } QBuiltin;

/* A call's argument as written: its name (null positional), mut. */
typedef struct QArg QArg;
struct QArg {
    QExpr *e;
    const QTok *name;
    const QTok *mut;
    QArg *next;
};

struct QExpr {
    QExprKind kind;
    QKind op;
    QType type;
    QType work;   /* QE_BINARY, QE_IN: the operands' type */
    bool untyped; /* QE_CONST: a literal's value, typed by its place */
    bool neg;     /* QE_IN: not in */
    bool trunc;   /* QE_CONV: int(x) of a float, toward 0 */
    QBuiltin builtin;
    const QTok *tok; /* its first token; a binary's, its operator */
    QExpr *a, *b, *c;
    QVal val;
    QVar *var;    /* QE_NAME's; QE_IN's temporary */
    QFn *fn;      /* QE_CALL's */
    QArg *args;   /* QE_CALL as written */
    QExpr **argv; /* QE_CALL: by parameter, or print's in order */
    uint32_t nargv;
    QExpr **items; /* QE_CHAIN */
    QKind *ops;
    QType *works;
    QVar **temps; /* each middle item's */
    uint32_t n;
};

/* ---- statements: one a line, in a flat list; a block's clauses linked */

typedef enum : uint8_t {
    QS_DECL, /* let, var, const: decls */
    QS_ASSIGN,
    QS_CALL,
    QS_IF, /* a block's; oneline: body */
    QS_ELSEIF,
    QS_ELSE,
    QS_ENDIF,
    QS_WHILE,
    QS_ENDWHILE,
    QS_FOR,
    QS_ENDFOR,
    QS_DO,
    QS_ENDDO,
    QS_BREAK,
    QS_CONTINUE,
    QS_RETURN,
    QS_FN,
    QS_ENDFN,
} QStmtKind;

/* A declaration's name [as type] [= value]. */
typedef struct QDecl QDecl;
struct QDecl {
    const QTok *name;
    const QTok *type;
    QExpr *value;
    QVar *var;
    QDecl *next;
};

struct QStmt {
    QStmtKind kind;
    QKind op;     /* QS_DECL: let, var or const; QS_ASSIGN: = or a compound's */
    bool oneline; /* QS_IF: then a statement on its line, body */
    bool broken;  /* a loop's: a break leaves it */
    bool early;   /* a module const's: checked before the rest */
    const QTok *tok;
    QStmt *next, *prev;
    /* if, elseif, else: the next clause (elseif, else, end if); a loop's opener: its end; an end:
       its opener; break, continue: the loop's opener */
    QStmt *link;
    QStmt *body;
    QExpr *e;         /* a condition, a call, a value, a for's start */
    QExpr *to, *step; /* a for's */
    QExpr *target;    /* QS_ASSIGN: a name */
    QDecl *decls;
    QFn *fn;   /* QS_FN, QS_ENDFN, QS_RETURN: its fn */
    QVar *var; /* QS_FOR: its counter, its limit and its step (null: constant) */
    QVar *lim, *stp;
    int32_t by; /* QS_FOR: its step when constant */
};

typedef struct {
    QSrc src;
    QStmt *first;
    QFn *fns;
    QVar *vars;
    QErrs errs;
} QProg;

/* ---- helpers */

void *qast_alloc(QMem *m, size_t size); /* zeroed */
/* *at, n entries of size, room for one more: grown by half. */
void *qast_room(QMem *m, void *at, uint32_t n, uint32_t *cap, size_t size);
QExpr *qast_expr(QMem *m, QExprKind kind, const QTok *tok);
const char *qast_typename(QType t);
/* The text a keyword or punctuation is spelled, for an error. */
const char *qast_spelling(QKind k);
static inline bool qast_numeric(const QType t)
{
    return t == QY_INT || t == QY_SINGLE || t == QY_DOUBLE;
}
static inline bool qast_float(const QType t) { return t == QY_SINGLE || t == QY_DOUBLE; }
#endif
