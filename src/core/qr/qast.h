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
    QK_FSTR,  /* an f-string's start: f" */
    QK_FPART, /* its text between values, escapes as written */
    QK_FSPEC, /* a value's format spec, after its : */
    QK_FEND,  /* its closing quote */
    /* keywords, in qlex.c's table's order */
    QK_AND,
    QK_AS,
    QK_BREAK,
    QK_CASE,
    QK_CONST,
    QK_CONTINUE,
    QK_DIV,
    QK_DO,
    QK_ELSE,
    QK_ELSEIF,
    QK_END,
    QK_ENUM,
    QK_FALSE,
    QK_FN,
    QK_FOR,
    QK_IF,
    QK_IN,
    QK_IS,
    QK_LET,
    QK_MATCH,
    QK_MOD,
    QK_MUT,
    QK_NONE,
    QK_NOT,
    QK_OR,
    QK_RETURN,
    QK_STEP,
    QK_THEN,
    QK_TRUE,
    QK_TYPE,
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
    QK_QQ, /* ?? */
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

/* A type: a scalar's QY_*, or from QY_FIRST a compound type's entry in QProg.tys. Lists, arrays
   and tuples are interned, so two types are the same when their QTypes are. */
typedef uint32_t QType;
enum : QType {
    QY_VOID,
    QY_INT,
    QY_SINGLE,
    QY_DOUBLE,
    QY_STRING,
    QY_BOOL,
    QY_ERROR, /* an error's: no further error about it */
    QY_FIRST = 8,
};

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

/* A type as written: a name, T[], T[n], (T, U), T?, T!. */
typedef enum : uint8_t { QX_NAME, QX_LIST, QX_ARRAY, QX_TUPLE, QX_OPT, QX_RES } QTypeExKind;
typedef struct QTypeEx QTypeEx;
struct QTypeEx {
    QTypeExKind kind;
    const QTok *tok; /* its first token: a name's its name */
    QTypeEx *elem;   /* a list's, an array's, T? and T!'s T */
    QExpr *count;    /* an array's */
    QTypeEx **items; /* a tuple's */
    uint32_t n;
};

/* A record's field, a tuple's member (no name), an enum value's datum. */
typedef struct {
    const QTok *name;
    QTypeEx *tx;
    QType type;
    QExpr *def;   /* its default, folded; null none */
    uint32_t off; /* its bytes into its record, qlink's */
} QField;

/* An enum's value: its data, their place in the enum's record. */
typedef struct {
    const QTok *name;
    QField *fields;
    uint32_t n;
    uint32_t first; /* its first datum's field in the enum's */
} QVariant;

typedef enum : uint8_t {
    QT_REC,
    QT_ENUM,
    QT_LIST,
    QT_ARRAY,
    QT_TUPLE,
    QT_MAP,
    QT_OPT,
    QT_RES
} QTyKind;

/* A compound type. A record, a tuple, an enum with data, a map, T? and T! are the VM's records
   (comp/declare.h): an enum's tag its field 0, then each value's data; a map's its keys, its values
   and its hash slots, three lists (qr/qrt.c); T?'s has and its T; T!'s err, its T, then its error's
   code, message and line (QF_*). */
typedef struct {
    QTyKind kind;
    const QTok *name; /* a record's, an enum's */
    QStmt *def;
    QType elem;     /* a list's, an array's, a map's values' */
    QType key;      /* a map's */
    uint32_t count; /* an array's */
    QField *fields; /* a record's, a tuple's; an enum's with data */
    uint32_t nfields, capfields;
    QVariant *vars; /* an enum's */
    uint32_t nvars, capvars;
    uint8_t state; /* a record's fields: 0 not typed, 1 being typed, 2 typed */
    bool data;     /* an enum some of whose values carry data */
    bool owns;     /* holds a list: moved, never copied by itself */
    bool laid;     /* qlink's: its TYPE's fields made */
    bool deep;     /* qlink's: its TYPE holds strings or lists (comp/declare.h decl_dynamic) */
    uint32_t td;   /* its TYPE (comp/declare.h), qlink's */
    uint32_t size; /* its TYPE's bytes, qlink's */
} QTy;

typedef struct {
    QTy *at;
    uint32_t n, cap;
} QTys;

/* ---- names */

typedef enum : uint8_t {
    QV_LET,
    QV_VAR,
    QV_CONST,
    QV_PARAM,  /* read-only, by address */
    QV_MUT,    /* a mut parameter */
    QV_LOOP,   /* a for's counter */
    QV_TEMP,   /* the compiler's: a chain's middle, a for's limit and step */
    QV_RESULT, /* a fn's result: a record's or a list's its last parameter */
    QV_EACH,   /* for x in a's x: a's element, by its index */
} QVarKind;

struct QVar {
    const QTok *name; /* its declaration's; a temporary's: its expression's first token */
    QType type;
    QVarKind kind;
    QFn *fn;       /* its fn, null the module's */
    QVal val;      /* a const's */
    bool constant; /* a const: val, untyped as its value's */
    bool untyped;
    bool mut;      /* QV_EACH: of a for mut */
    bool shared;   /* a module's, used in a fn: never moved */
    uint32_t last; /* its last use's token, sema's */
    QExpr *place;  /* QV_EACH, a case's datum: what it names, checked */
    uint32_t id;   /* its variable (mem/symbols.h), qlink's */
    QVar *next;  /* the program's next */
};

typedef struct {
    const QTok *name;
    QTypeEx *type; /* its as type, null none */
    QExpr *def;    /* its default, folded; null none */
    bool mut;
    QVar *var;
} QParam;

struct QFn {
    const QTok *name;
    QParam *params;
    uint32_t npar;
    QTypeEx *rtype; /* its as type, null none */
    QType result;
    bool byarg; /* its result a record, tuple or list: written through its last parameter */
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
    QE_CALL,  /* tok's fn or built-in with args, a the receiver of a.f(args); argv by parameter */
    QE_CONV,  /* a converted to type */
    QE_FIELD, /* a.tok: field n of a record, tuple or enum */
    QE_INDEX, /* a[b]: a list's element, a map's value by its key */
    QE_NEW,   /* a record, tuple or enum value of type: argv by field, null its default */
    QE_TUPLE, /* (items) as written: return a, b */
    QE_FSTR,  /* f"..": items its parts, a string's or a value's, specs each value's */
    QE_LIST,  /* a list's built-in: builtin, a the list, argv */
    QE_HAS,   /* a in b, b a list; not in when neg */
    QE_SLICE, /* a[b..c], b or c null when left out */
    QE_NONE,  /* none */
    QE_IS,    /* a is none, a is Error (op QK_NONE or QK_NAME); is not when neg */
    QE_TRY,   /* a?: a's value, or its error returned from fn */
} QExprKind;

/* A built-in: print, a conversion, len, copy, a list's methods. */
typedef enum : uint8_t {
    QB_NONE,
    QB_PRINT,
    QB_INT,
    QB_SINGLE,
    QB_DOUBLE,
    QB_STRING,
    QB_LEN,
    QB_COPY,
    QB_PUSH,
    QB_POP,
    QB_INSERT,
    QB_REMOVE,
    QB_BYTE,  /* s[i]: a, b */
    QB_SLICE, /* a[b..c] of a string or a list: b, c null for its ends */
    QB_GET,   /* m.get(k): a, b; its value's address in temps[0] */
} QBuiltin;

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
    bool insert;  /* QE_INDEX of a map, assigned: its key added when not there */
    QBuiltin builtin;
    const QTok *tok; /* its first token; a binary's, its operator */
    QExpr *a, *b, *c;
    QVal val;
    QVar *var;    /* QE_NAME's; QE_IN's temporary; a record's built there first */
    QFn *fn;      /* QE_CALL's */
    QArg *args;   /* QE_CALL as written */
    QExpr **argv; /* QE_CALL: by parameter, or print's in order */
    uint32_t nargv;
    QExpr **items; /* QE_CHAIN */
    QKind *ops;
    QType *works;
    QVar **temps; /* each middle item's */
    const QTok **specs; /* QE_FSTR: each value's format spec, null none */
    uint32_t n;
    uint32_t field; /* QE_FIELD: its field's index; QE_NEW: an enum's value */
};

/* A match's case: one or more patterns, each a value, a to b, is op v, or an enum value with its
   data's names; else alone. */
typedef enum : uint8_t { QP_VALUE, QP_RANGE, QP_IS, QP_VARIANT } QPatKind;
typedef struct QPat QPat;
struct QPat {
    QPatKind kind;
    const QTok *tok;
    QKind op;           /* QP_IS's */
    QExpr *a, *b;       /* the value; a range's ends */
    const QTok **binds; /* QP_VARIANT: its data's names */
    uint32_t nbinds;
    QVar **vars;
    uint32_t variant; /* QP_VARIANT's, sema's */
    QPat *next;
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
    QS_TYPE, /* type Name: its fields' lines follow */
    QS_FIELD,
    QS_ENDTYPE,
    QS_ENUM,
    QS_VARIANT, /* an enum's value: decls its data */
    QS_ENDENUM,
    QS_MATCH, /* match e: its cases' lines follow, each linked to the next */
    QS_CASE,  /* pats, or case else (none) */
    QS_ENDMATCH,
} QStmtKind;

/* A declaration's name [as type] [= value]. */
typedef struct QDecl QDecl;
struct QDecl {
    const QTok *name;
    QTypeEx *type;
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
    QExpr *target;    /* QS_ASSIGN: a place, or the QE_TUPLE of a, b = .. */
    QStmt *pre;       /* QS_ASSIGN, compound: its target's subscripts' temporaries, set first */
    QDecl *decls;
    QFn *fn;   /* QS_FN, QS_ENDFN, QS_RETURN: its fn */
    QVar *var; /* QS_FOR: its counter, its limit and its step; over a list: its element */
    QVar *lim, *stp;
    QVar *index; /* QS_FOR over a list: its index (i in for i, x in a), else the compiler's */
    QVar *temp;  /* QS_MATCH: its value's; QS_ASSIGN of a tuple: each value's in temps */
    QVar **temps;
    int32_t by;   /* QS_FOR: its step when constant, else 0 */
    bool each;    /* QS_FOR over a list or an array, e */
    bool mut;     /* for mut x in a */
    QPat *pats;   /* QS_CASE's; none: case else; its match its body */
    QType ty;     /* QS_TYPE, QS_ENUM: its type; QS_MATCH: its value's */
    QExpr *place; /* QS_MATCH of an enum with data: its value's place, its data read there */
    bool full;    /* QS_MATCH: every value has its case */
};

typedef struct {
    QSrc src;
    QStmt *first;
    QFn *fns;
    QVar *vars;
    QTys tys;
    QErrs errs;
} QProg;

/* ---- helpers */

void *qast_alloc(QMem *m, size_t size); /* zeroed */
/* *at, n entries of size, room for one more: grown by half. */
void *qast_room(QMem *m, void *at, uint32_t n, uint32_t *cap, size_t size);
QExpr *qast_expr(QMem *m, QExprKind kind, const QTok *tok);
/* A scalar type's name. */
const char *qast_typename(QType t);
/* Compound type t's entry. */
static inline QTy *qast_ty(const QProg *const p, const QType t) { return &p->tys.at[t - QY_FIRST]; }
static inline bool qast_compound(const QType t) { return t >= QY_FIRST; }
/* Is t the VM's record: a record, a tuple, an enum with data, a map, T?, T!? */
enum { QF_FLAG, QF_VAL, QF_CODE, QF_MESSAGE, QF_LINE }; /* T? and T!'s fields */
static inline bool qast_record(const QProg *const p, const QType t)
{
    if (!qast_compound(t))
        return false;

    const QTy *const y = qast_ty(p, t);
    return y->kind == QT_REC || y->kind == QT_TUPLE || y->kind == QT_MAP || y->kind == QT_OPT ||
           y->kind == QT_RES || (y->kind == QT_ENUM && y->data);
}
/* Is t a list or a fixed array: the VM's array? */
static inline bool qast_array(const QProg *const p, const QType t)
{
    return qast_compound(t) && (qast_ty(p, t)->kind == QT_LIST || qast_ty(p, t)->kind == QT_ARRAY);
}
/* Is t a plain enum: an int? */
static inline bool qast_tag(const QProg *const p, const QType t)
{
    return qast_compound(t) && qast_ty(p, t)->kind == QT_ENUM && !qast_ty(p, t)->data;
}
/* Is e a variable's place: a name, its fields', its elements'? */
static inline bool qast_place(const QExpr *e)
{
    while (e->kind == QE_FIELD || e->kind == QE_INDEX)
        e = e->a;

    return e->kind == QE_NAME && e->var;
}
/* Is e a value made fresh, no place's: a call's, a record's built? */
static inline bool qast_fresh(const QExpr *const e)
{
    return e->kind != QE_NAME && e->kind != QE_FIELD && e->kind != QE_INDEX;
}
/* The text a keyword or punctuation is spelled, for an error. */
const char *qast_spelling(QKind k);
static inline bool qast_numeric(const QType t)
{
    return t == QY_INT || t == QY_SINGLE || t == QY_DOUBLE;
}
static inline bool qast_float(const QType t) { return t == QY_SINGLE || t == QY_DOUBLE; }
#endif
