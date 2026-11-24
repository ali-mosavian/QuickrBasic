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

/* A source line: its record in the program store, its text (the arena's copy), its number, its
   module (QProg.mods). */
typedef struct {
    uint32_t at;
    const char *text;
    uint32_t len;
    uint16_t src;
    uint16_t mod;
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
    QK_YIELD,
    QK_YIELDS,
    QK_FROM,
    QK_IMPORT,
    QK_PUBLIC,
    QK_NATIVE,
    QK_OPERATOR,
    QK_TEST,
    QK_ASSERT,
    QK_SHL,
    QK_SHR,
    QK_TO,
    QK_NEXT, /* only where a statement starts: elsewhere a name, it.next() */
    QK_WEND,
    QK_LOOP,
    QK_UNTIL,
    QK_SELECT,
    QK_PRINT,
    QK_SELF,
    QK_PRIVATE,
    QK_FUNCTION,
    QK_MATCH, /* not a keyword: an error naming select case */
    /* punctuation */
    QK_LPAREN,
    QK_RPAREN,
    QK_LBRACK,
    QK_RBRACK,
    QK_LBRACE,
    QK_RBRACE,
    QK_COMMA,
    QK_COLON,
    QK_SEMI,
    QK_DOT,
    QK_DOTDOT,
    QK_QUEST,
    QK_ORELSE, /* a or else b: the parser's, no token */
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
    QK_BAND, /* and, or, xor, not of integers: sema's, no token */
    QK_BOR,
    QK_BXOR,
    QK_BNOT,
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
    bool big; /* QK_INT past INT64_MAX: i its bits, a ulong's */
} QTok;

/* The source: every module's lines; the tokens of the lines lexed last, a QK_EOF after them. */
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
/* The errors in order of line and column, each once. */
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
    QY_BYTE,
    QY_SHORT,
    QY_LONG,
    QY_UBYTE,
    QY_USHORT,
    QY_UINT,
    QY_ULONG,
    QY_FIRST = 16,
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
    bool big; /* an untyped integer past INT64_MAX: i its bits, a ulong's */
} QVal;

typedef struct QVar QVar;
typedef struct QFn QFn;
typedef struct QLam QLam;
typedef struct QCap QCap;
typedef struct QExpr QExpr;
typedef struct QStmt QStmt;
/* The names in sight at a statement, and qsem's own kept past its pass: the debugger's (qsem.c). */
typedef struct QScopeAt QScopeAt;
typedef struct QSemKeep QSemKeep;
typedef struct QIter QIter;

/* A frame's owners of values with drop, a fn's or the modules': its lets and vars, in order, each
   dropped when it ends (QVar.live); a drop loop's index variable for each depth of lists. */
typedef struct {
    QVar **owned;
    uint32_t nowned, capowned;
    QVar **ix;
    uint32_t nix;
} QOwners;

/* A type as written: a name, T[], T[n], (T, U), T or none, T or Error, function(T) as U,
   dict[K, V], Name[T, U] (a generic record's, or Name[N] an array's when Name is not generic). */
typedef enum : uint8_t {
    QX_NAME,
    QX_LIST,
    QX_ARRAY,
    QX_TUPLE,
    QX_OPT,
    QX_RES,
    QX_FN,
    QX_DICT,
    QX_GEN
} QTypeExKind;
typedef struct QTypeEx QTypeEx;
struct QTypeEx {
    QTypeExKind kind;
    const QTok *tok; /* its first token: a name's its name */
    QTypeEx *elem;   /* a list's, an array's, an or's T, a dict's V; a fn's result, null none; a
                        generic's name */
    QExpr *count;    /* an array's */
    QTypeEx **items; /* a tuple's; a fn's parameters'; a dict's K; a generic's types */
    uint32_t n;
    const QTok **quals; /* a name's modules before it: gfx.Window's gfx */
    uint32_t nquals;
};

/* A record's field, a tuple's member (no name), an enum value's datum. */
typedef struct {
    const QTok *name;
    QTypeEx *tx;
    QType type;
    QExpr *def;   /* its default, folded; null none */
    uint32_t off; /* its bytes into its record, qlink's */
    bool priv;    /* private: its module's alone */
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
    QT_RES,
    QT_FN
} QTyKind;

/* A compound type. A record, a tuple, an enum with data, a map, T? and T! are the VM's records
   (comp/declare.h): an enum's tag its field 0, then each value's data; a map's its keys, its values
   and its hash slots, three lists (qr/qrt.c); T?'s has and its T; T!'s err, its T, then its error's
   code, message and line (QF_*). A fn type's fields are its parameters', its elem its result's
   (QY_VOID none): its value a procedure's number, an int. A kept generator's state is a record,
   gen its resumable instance: its parameters, its resume index, its locals. */
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
    bool boxed;    /* qlink's: a fixed array, or a list that is an element: its TYPE's one field */
    uint32_t td;   /* its TYPE (comp/declare.h), qlink's */
    uint32_t size; /* its TYPE's bytes, qlink's */
    uint8_t *word; /* qlink's: by offset in its first QAST_WORDS bytes, the number + 1 of the
                      number field there, a held record's too (a word); 0 none */
    uint32_t nwords;
    QFn *gen;    /* a kept generator's state's: its resumable instance */
    QFn **opers; /* a record's fn operators: those one of whose values it is */
    uint32_t nopers, capopers;
    QFn *drop;   /* a record's fn drop */
    QFn **meths; /* a record's or an enum's methods */
    uint32_t nmeths, capmeths;
    bool seen;            /* qast_drops' guard against a type holding itself */
    const QTok **tparams; /* type Name[T, U]'s, its instances' too: no fields, none typed */
    uint32_t ntp;
    QType of;     /* an instance's generic */
    QType *targs; /* an instance's types for its tparams */
} QTy;

enum { QAST_WORDS = 256 }; /* a record in cells' bytes whose fields are variables (qr/qlink.c) */

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
    bool mut;       /* QV_EACH: of a for mut */
    bool shared;    /* a module's, used in a fn: never moved */
    uint32_t last;  /* its last use's token, sema's */
    QExpr *place;   /* QV_EACH, a case's datum: what it names, checked */
    uint32_t id;    /* its variable (mem/symbols.h), qlink's: an inline record's lowest cell */
    uint32_t words; /* an inline record's: its first word's variable, qlink's */
    bool cells;     /* a record in cells, qlink's; else a box */
    QVar *next;    /* the program's next */
    QCap *cap;     /* a lambda's hidden parameter: the name it stands for, by reference */
    QLam *lam;     /* an instance's parameter bound to this lambda: no value */
    bool byref;    /* a resumable generator's: kept by its holder, a parameter by reference */
    QVar *live;    /* an owner of a value with drop: 1 while it holds one */
};

typedef struct {
    const QTok *name;
    QTypeEx *type; /* its as type, null none */
    QExpr *def;    /* its default, folded; null none */
    bool mut;
    bool hidden; /* a capture's, after the rest: by reference */
    QVar *var;
    QLam *lam;      /* an instance's, bound to this lambda: no value passed */
    uint32_t capat; /* bound: its lambda's first hidden parameter */
} QParam;

struct QFn {
    const QTok *name;
    bool pub;
    QStmt *in; /* a method's type or enum, its statement; null a function */
    bool self; /* a method on a value: its first parameter self */
    bool priv; /* a private method: its module's alone */
    QParam *params;
    uint32_t npar;
    QTypeEx *rtype; /* its as type, null none */
    QType result;
    bool byarg; /* its result a record, tuple or list: written through its last parameter */
    bool sig;  /* its signature resolved */
    QVar *res; /* its result's variable, null none */
    QStmt *def, *end;
    uint32_t proc;          /* its procedure (comp/blocks.h), qlink's */
    QFn *next;              /* the program's next */
    bool native;            /* native fn: no body, its calls the entry's code */
    const struct QNat *nat; /* a native's entry (qnat.h) */
    const QTok **tparams; /* fn f[T, U]: its type parameters */
    uint32_t ntp;
    QType *targs;  /* an instance's types for them */
    const QFn *of; /* an instance's generic */
    QFn *insts;    /* a generic's instances, by sibling */
    QFn *sibling;
    uint32_t ninst;   /* a generic's instances made */
    QLam **binds;     /* an instance's lambda for each parameter, null none */
    uint32_t nhidden; /* its last parameters a capture's each */
    QLam *lamof;      /* a block lambda's procedure */
    QLam *aslam;      /* it passed as a lambda, named */
    bool inl;         /* a one-line lambda's, for its site's check: no code, no variables */
    QTypeEx *ytx;     /* function g(..) yields T: a generator */
    QType ytype;
    QStmt **yields; /* an instance's yields, numbered from 1 */
    uint32_t nyields, capyields;
    QStmt *loop;    /* for x in g(..)'s instance: that for, its code in its host's frame */
    QFn *host;      /* an inline or resumable instance's fn holding its state, null the module */
    bool resumable; /* var it = g(..)'s instance: a procedure, its state a record (gentype) */
    QVar *resume;   /* the yield to go on from: 0 the start, -1 done */
    QExpr **keep;   /* resumable: a lambda's capture, what it is passed; null the rest */
    QType gentype;  /* resumable: its state, a record of each parameter (QY_INT a lambda's) */
    uint32_t nown;  /* resumable: its parameters as written, its state's after them */
    QExpr **news; /* resumable: each state made, g(..), its fields after nown filled by qsem_keep */
    uint32_t nnews, capnews;
    QKind op;         /* fn operator +: its operator, its name its token */
    const QTok *lone; /* an operator's or a method's body one return: its value's first token */
    bool inlining;    /* its lone value being checked where it is used */
    QOwners own;
    bool test;     /* a test block: its name a string's token; a procedure under --test alone */
    QStmt *onfail; /* a test's QS_TESTFAIL */
    QVar **writes; /* the module vars its body writes */
    uint32_t nwrites, capwrites;
    QFn **callees; /* the fns its body calls: its block lambdas' and its owners' drops too */
    uint32_t ncallees, capcallees;
    bool icalls;   /* it calls through a fn value */
    uint32_t walk; /* the last search of its callees' writes it was in, sema's */
    bool idle;     /* a generic record instance's method not called: unchecked, no code */
    bool checked;  /* its body checked, sema's */
};

/* A name a lambda uses from outside it, as its site bound it. A fn's storage (frame) is reached
   through a hidden parameter by reference (param, the lambda's own); the rest are seen as they
   are. */
struct QCap {
    const QTok *name;
    QVar *var;
    QFn *fn;
    QType ty;
    uint32_t at; /* its binding's index at the site, sema's */
    QVar *param; /* frame's: the lambda's parameter for it */
    bool frame;
    bool mut; /* the lambda changes it */
};

/* fn(x) expr, or a block's fn(x as T) .. end fn: its parameters typed by the place it is passed to.
   A block is a procedure, fn; a one-line's body is checked again where it is called, inlined. A
   named fn passed as one, named, is called directly. */
struct QLam {
    const QTok *tok; /* its fn */
    const QTok *at;  /* a one-line's body's first token */
    QParam *params;
    uint32_t npar;
    QTypeEx *rtype; /* a block's as type, null none */
    QExpr *body;    /* a one-line's, as its site checked it */
    QFn *fn;        /* a block's; a one-line's site check's */
    QFn *named;
    QCap **caps;
    uint32_t ncaps, capcaps;
    QType type;   /* its fn type */
    bool checked; /* at its site, clean */
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
    QE_LAMBDA, /* lam, a value only for a fn's parameter */
    QE_APPLY,  /* one-line lam inlined: argv each stored to temps[k] (null: a place, read there),
                  then a */
    QE_FNREF,  /* fn as a value: its procedure's number */
    QE_RANGE,  /* (a..b), a chain's source only */
    QE_ITER,   /* a chain, iter: one loop */
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
    QB_ICALL, /* a(argv): a call through fn value a */
    QB_SWAP,  /* a.swap(i, j): argv */
    QB_SORT,  /* a.sort(): by value; a.sort(lam): argv[0] the lambda on a[temps[0]], a[temps[1]] */
    QB_NONETEXT, /* "none", a T or none's text */
} QBuiltin;

/* A call's argument as written: its name (null positional), mut. */
typedef struct QArg QArg;
struct QArg {
    QExpr *e;
    const QTok *name;
    QKind sep; /* print's: ; or , after it; QK_EOF none */
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
    bool done;    /* checked already: a generic call's argument, its T read from it */
    bool wrap;    /* QE_BINARY, QE_UNARY, QE_CONV in wrap(..): integers wrap, not Overflow */
    bool move;    /* QE_NAME: an owner's value moved out, its live flag cleared */
    bool copy;    /* an owning place's value taken whole: copied, never moved */
    bool byval;   /* an argument by value whose variable the call may change: its value passed */
    QBuiltin builtin;
    const QTok *tok; /* its first token; a binary's, its operator */
    QExpr *a, *b, *c;
    QVal val;
    QVar *var; /* QE_NAME's; QE_IN's temporary; a record's built there first; a for's x: its copy */
    QFn *fn;   /* QE_CALL's */
    QArg *args;   /* QE_CALL as written */
    QExpr **argv; /* QE_CALL: by parameter, or print's in order */
    uint32_t nargv;
    QExpr **items; /* QE_CHAIN */
    QKind *ops;    /* QE_CHAIN's; print's: each value's separator after it, QK_EOF none */
    QType *works;
    QVar **temps; /* each middle item's */
    const QTok **specs; /* QE_FSTR: each value's format spec, null none */
    uint32_t n;
    uint32_t field; /* QE_FIELD: its field's index; QE_NEW: an enum's value */
    QLam *lam;      /* QE_LAMBDA's, QE_APPLY's */
    QIter *iter;    /* QE_ITER's */
    QTypeEx *tx;    /* Name[T](..)'s generic record, a QE_CALL's; Name[T].f's, a QE_INDEX's when it
                       names one */
};

/* ---- chains: a.map(f).filter(p).sum(), one loop */

/* A chain's step: map(f) and filter(p) take a lambda or a fn, a one-line lambda inlined; take(n),
   skip(n) count; zip(b) adds b's element to each. */
typedef enum : uint8_t { QI_MAP, QI_FILTER, QI_TAKE, QI_SKIP, QI_ZIP } QStageKind;
typedef struct {
    QStageKind kind;
    QExpr *e;    /* map's value, filter's condition, take's and skip's count, zip's element */
    QVar *x;     /* map's value's or zip's element's variable; null: read where it is */
    QVar *n, *k; /* take, skip: the count and how many so far; zip: b's index and its last */
    QExpr *of;   /* zip: b, a list's or a string's place, a range's start */
    QExpr *to;   /* zip: a range's end */
    bool str;    /* zip: b a string */
    bool win;    /* take, skip, zip before a rev(): a window on the source's indices, set at its
                    start; zip's k then b's index less the source's */
} QStage;

/* What a chain runs over: a list's or an array's elements (a map's keys), a range's ints, a
   string's bytes, what a kept generator yields. */
typedef enum : uint8_t { QO_LIST, QO_RANGE, QO_STRING, QO_GEN } QSource;
/* What uses a chain up: a reducing method, collect() or a for. */
typedef enum : uint8_t {
    QZ_SUM,
    QZ_MIN,
    QZ_MAX,
    QZ_COUNT,
    QZ_ANY,
    QZ_ALL,
    QZ_FIND,
    QZ_FOLD,
    QZ_COLLECT,
    QZ_FOR,
} QSink;

struct QIter {
    QSource from;
    QSink sink;
    bool rev; /* counted down */
    const QTok *tok;
    QExpr *src; /* the source's value held first: a fresh list's built, a string's in hold */
    QVar *hold;
    QExpr *list;         /* a list's or a string's place, the elements read there */
    QExpr *lo, *hi;      /* a counted source's range: a..b, or 0..len */
    QVar *i, *lim, *stp; /* its counter, its last and its step */
    QExpr *start;        /* a generator made here, g(..): its arguments kept, gen from the start */
    QVar *gen;
    QExpr *next; /* a generator's next(), to got */
    QVar *got;
    QStage *stages;
    uint32_t n;
    QExpr **parts; /* the element where it is used up: a zip's its parts */
    uint32_t nparts;
    QExpr *pred; /* count(p), any, all, find: the condition; fold's next value */
    QExpr *push; /* collect's */
    QVar *acc, *has;
    QExpr *init;   /* acc's first value: a sum's 0, fold's */
    QExpr *result; /* after the loop; null: built in acc, the QE_ITER's var */
    QVar **xs;     /* a for's names, each a part's */
    uint32_t top;  /* the loop's start: its label's body, or its code's offset (qr/qcg.c) */
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
    QS_ENDDO, /* e: loop while e (op QK_WHILE), loop until e (QK_UNTIL); null none */
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
    QS_SELECT, /* select case e: its cases' lines follow, each linked to the next */
    QS_CASE,  /* pats, or case else (none) */
    QS_ENDSELECT,
    QS_YIELD,    /* e; link: the statement on its line (itself, or its one-line if) */
    QS_IMPORT,   /* import a.b [as n], ..: imps */
    QS_FROM,     /* from a.b import X [as Y], ..: imps the module, names what it gives */
    QS_ATEND,    /* the program's end: each module owner dropped; a line of its own */
    QS_ASSERT,   /* assert e */
    QS_TESTRUN,  /* --test: test fn run, counted passed unless it fails; link its QS_TESTFAIL */
    QS_TESTFAIL, /* fn's error handler: counted failed, said, then to its end */
    QS_TESTEND,  /* the tests' tally said; the run's end */
} QStmtKind;

/* A module named by import or from: its dotted name, the folders its leading dots go up (0 the
   search path's, 1 the importing file's folder), its as; the module found (qr.c), QMOD_NONE not
   found. A name from gives: path its one token; mod its module when from's is a package's
   folder. */
enum : uint32_t { QMOD_NONE = UINT32_MAX, QMOD_QBASIC = UINT32_MAX - 1 };
typedef struct QImport QImport;
struct QImport {
    const QTok *tok; /* its first */
    const QTok **path;
    uint32_t npath;
    uint32_t up;
    const QTok *as;
    uint32_t mod;
    bool folder; /* from's module a package's folder */
    QImport *next;
};

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
    uint32_t line; /* its QLine, its code's: tok's, or an instance's own copy of it (qr.c) */
    QStmt *next, *prev;
    /* if, elseif, else: the next clause (elseif, else, end if); a loop's opener: its end; an end:
       its opener; break, continue: the loop's opener */
    QStmt *link;
    QStmt *body;
    QStmt *other;     /* QS_IF, one line: its else's statement, null none */
    QExpr *e;         /* a condition, a call, a value, a for's start */
    QExpr *to, *step; /* a for's */
    QExpr *target;    /* QS_ASSIGN: a place, or the QE_TUPLE of a, b = .. */
    QStmt *pre;       /* QS_ASSIGN, compound: its target's subscripts' temporaries, set first */
    QDecl *decls;
    QFn *fn;   /* QS_FN, QS_ENDFN, QS_RETURN: its fn */
    QVar *var; /* QS_FOR: its counter, its limit and its step; over a list: its element */
    QVar *lim, *stp;
    QVar *index; /* QS_FOR over a list: its index (i in for i, x in a), else the compiler's */
    QVar *temp;  /* QS_SELECT: its value's; QS_ASSIGN of a tuple: each value's in temps */
    QVar **temps;
    int32_t by;         /* QS_FOR: its step when constant, else 0; QS_YIELD: its number */
    bool each;          /* QS_FOR over a list or an array, e */
    bool mut;           /* for mut x in a */
    bool inclusive;     /* QS_FOR: for i = a to b, b counted */
    QPat *pats;         /* QS_CASE's; none: case else; its match its body */
    QType ty;           /* QS_TYPE, QS_ENUM: its type; QS_SELECT: its value's */
    QExpr *place;       /* QS_SELECT of an enum with data: its value's place, its data read there */
    bool full;          /* QS_SELECT: every value has its case */
    bool pub;           /* public: a module's fn, type, enum, const, var or let */
    bool priv;          /* QS_FIELD: private */
    QImport *imps;      /* QS_IMPORT's modules; QS_FROM's module */
    QImport *names;     /* QS_FROM's */
    QOwners *own;       /* its frame's: a fn's, or the modules' */
    const QScopeAt *at; /* the names in sight at it, QProg.keep */
    const QTok **tparams; /* QS_TYPE: type Name[T, U]'s; an instance's none (ntp 0) */
    uint32_t ntp;
};

/* A file: a module. Its path as found, its lines (from first), its tokens (base: the first's
   number across modules), its statements; the modules it imports, by its import statements. */
typedef struct {
    const char *path;
    uint32_t first, nlines;
    QTok *toks;
    uint32_t ntoks, base;
    QStmt *head, *tail;
} QMod;

typedef struct {
    QSrc src;
    QMod *mods; /* [0] the main program */
    uint32_t nmods, capmods;
    uint32_t *order; /* the modules in the order their vars start: each after those it imports */
    uint32_t end; /* the editor's: the line the run goes on to past the main's, + 1; 0 the next */
    QStmt *first;
    QFn *fns;
    QVar *vars;
    QTys tys;
    QErrs errs;
    QErrs copies; /* each copy of a value owning a list kept, for --copies: "a T copied" */
    QOwners own;  /* the modules' */
    QStmt *atend; /* QS_ATEND, when a module owns a value with drop */
    bool testing; /* --test: the tests run, the main's top level only declares */
    bool loose;   /* the editor's line check: a block's lines need not pair */
    QStmt **runs; /* --test: each test's QS_TESTRUN, the modules' in their order */
    uint32_t nruns, capruns;
    QStmt *testend;
    QVar *failed, *passes, *fails; /* the test running's; the tally */
    bool keep;                     /* the debugger's: each statement's names in sight kept */
    QSemKeep *sem;
} QProg;

/* ---- helpers */

/* Type y's parts that may hold a value with drop: a record's or a tuple's fields, a list's or an
   array's element, T?'s or T!'s value. */
uint32_t qast_parts(const QTy *y);
QType qast_part(const QTy *y, uint32_t k);
/* Does a value of type t hold one with drop: a record with fn drop, or a record, tuple, list,
   array, T? or T! holding one? */
bool qast_drops(const QProg *p, QType t);
/* How deep in lists a value of type t holds one with drop: its drop's loops' nesting. */
uint32_t qast_dropdepth(const QProg *p, QType t);

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
static inline bool qast_isfn(const QProg *const p, const QType t)
{
    return qast_compound(t) && qast_ty(p, t)->kind == QT_FN;
}
/* Is compound t an int cell: a plain enum, a fn value? */
static inline bool qast_cell(const QProg *const p, const QType t)
{
    return qast_tag(p, t) || qast_isfn(p, t);
}
/* Is e a variable's place: a name, its fields', its elements'? */
static inline bool qast_place(const QExpr *e)
{
    while (e->kind == QE_FIELD || e->kind == QE_INDEX)
        e = e->a;

    return e->kind == QE_NAME && e->var;
}
/* Is f a generic's or a generator's definition, or a generic record's method: no code, each
   instance checked and compiled? */
static inline bool qast_generic(const QFn *const f)
{
    return ((f->ntp || f->ytx) && !f->of) || (f->in && f->in->ntp);
}
/* Is f for x in g(..)'s instance: no procedure, its code its host's? */
static inline bool qast_inline(const QFn *const f) { return f->loop != nullptr; }
/* The fn whose frame holds f's variables: f, or an inline instance's host's; null the module. */
static inline const QFn *qast_home(const QFn *f)
{
    while (f && qast_inline(f))
        f = f->host;

    return f;
}
/* Is e a value made fresh, no place's: a call's, a record's built? */
static inline bool qast_fresh(const QExpr *const e)
{
    return e->kind != QE_NAME && e->kind != QE_FIELD && e->kind != QE_INDEX;
}
/* The text a keyword or punctuation is spelled, for an error. */
const char *qast_spelling(QKind k);
static inline bool qast_integer(const QType t)
{
    return t == QY_INT || (t >= QY_BYTE && t <= QY_ULONG);
}
static inline bool qast_unsigned(const QType t) { return t >= QY_UBYTE && t <= QY_ULONG; }
static inline bool qast_float(const QType t) { return t == QY_SINGLE || t == QY_DOUBLE; }
static inline bool qast_numeric(const QType t) { return qast_integer(t) || qast_float(t); }
/* An integer type's bits. */
static inline uint32_t qast_bits(const QType t)
{
    return t == QY_BYTE || t == QY_UBYTE     ? 8
           : t == QY_SHORT || t == QY_USHORT ? 16
           : t == QY_LONG || t == QY_ULONG   ? 64
                                             : 32;
}
/* Is t a long or a ulong: an 8-byte cell, its arithmetic CALL QX's? */
static inline bool qast_wide(const QType t) { return t == QY_LONG || t == QY_ULONG; }
#endif
