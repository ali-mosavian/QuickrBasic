/* The compiler's interface. How it works: comp.c. */
#ifndef COMP_COMP_H
#define COMP_COMP_H
#include <setjmp.h>
#include "defs.h"
#include "comp/code.h"
#include "vm/exec.h"
#include "io/console.h"

/* The compiled expression's target (comp.tgtsubs): FFh a record's field (ctrl.field). */
enum : uint8_t { TGT_FIELD = 0xFF };

enum { COMP_TAIL = 12 }; /* room past the code's limit: a line's ERR and EOL */

typedef struct {
    /* the cursor and the code */
    const uint8_t *p;    /* (ESI) the token cursor */
    uint8_t *map;        /* cmap: the statement map's top; its Stmt entries grow down */
    uint32_t nstmt;      /* statements recorded */
    const uint8_t *body; /* cbase: the body's start */
    uint32_t line;       /* the line's offset in the program store, DLINE direct */
    uint8_t len;         /* xo: the code's length, mod 256 */
    bool busy;           /* csp: compiling; an error is comp_fail's */
    jmp_buf unwind;      /* csp: comp_fail's way back to comp_text */
    uint16_t depth;      /* cplim: expression nesting, Out of memory past COMP_MAXDEPTH */

    /* the expression */
    bool folding; /* g_cfold: fold_code's run; an error is its */
    jmp_buf fold; /* g_cfsp: fold_code's way back from an error */

    /* the target of a store */
    uint16_t tgt;    /* ctv: its variable, or a record field's offset */
    uint8_t tgtinfo; /* ctk: its TVAR.info */
    uint8_t tgtsubs; /* ctn: its subscripts, TGT_FIELD a record's field */

    /* the statement */
    const uint8_t *kw;        /* ctks: its keyword */
    uint8_t tok;              /* ctok: that token (LOAD, SAVE, VERIFY) */
    bool then;                /* cth: it follows a THEN */
    bool oneif;               /* csif: in a one-line IF: ELSE ends a statement */
    uint8_t *dbg;             /* cdbg: past its DBG op, null none */
    uint8_t nargs;            /* ccnt: a CALL's counted arguments */
    uint8_t forkind;          /* cfk: FOR's variable's kind, 0 a FLOAT */
    uint16_t forvar;          /* cfid: FOR's variable */
    struct Node *on, *onlast; /* ON's node, its last line */
    uint16_t redim;           /* credim: DIM_REDIM, DIM_PRESERVE or 0 */
    uint16_t lnum;            /* wv: a jump's line number, NONUM a label's */
    uint32_t jline;           /* jline: its line, 0 until the first jump finds it */
    bool modlabel;            /* g_lmod: comp_lnum's label is the module's (cleared) */

    /* scopes */
    uint8_t proc;   /* cproc: the procedure the line is in, 0 the module (mem/scope sets it) */
    uint8_t scope;  /* cscope: var_find's scope: 0 the module, a procedure */
    bool isstatic;  /* cstat: var_find makes a STATIC, not a local */
    uint8_t dlproc; /* g_dlprc: the direct line's procedure (the debugger's probe) */
    bool check;     /* cchk: the IDE's check of a line alone, unlinked */

    /* the error */
    uint8_t dg;   /* cdg: its DG_ (io), 0 by its text */
    uint16_t dfa; /* cdfa: the automaton's state it failed in, FFFFh none */

    uint64_t dmask; /* dimsubs: bit i, subscript i its hi only */
    /* A typed line's body, LMAX+64 bytes after the byte before it (Line.sep's place), carved
       from the arena at start: a literal's Addr points into it. */
    uint8_t *cbuf;
    /* The compiled code, before link_xlate streams it into the cache. */
    uint8_t xbuf[XLIM + COMP_TAIL];
} Comp;
/* TVAR.info's ET_* to its kind: 0 FLOAT, 1 INT, 2 STRING, SK_DBL DOUBLE. */
extern const uint8_t comp_etkind[7];

/* cplim's CPROOM bytes of the asm's stack as nesting levels of expr's prec (24 bytes a
   parenthesis); to be calibrated against the asm build. */
enum { COMP_MAXDEPTH = CPROOM / 24 };

/* A body's tokens: M_INT n:4; M_VAR len text id:2 (the text with its suffix, if any). */
enum { INT_TOKEN_SIZE = 5 };
/* Past the name token at p. */
static inline const uint8_t *comp_skipname(const uint8_t *const p) { return p + p[1] + 4; }
/* The name token at p's suffix (% & ! # $, all under '.'), 0 none. */
static inline uint8_t comp_suffixof(const uint8_t *const p)
{
    const uint8_t c = p[p[1] + 1];
    return c < '.' ? c : 0;
}

/* Compiling */
void comp_init(Session *S); /* comp_cbuf carved (allo_carve) */
/* Procedure proc compiled, 0 the module: its scopes seen. */
void comp_proc(Session *S, uint8_t proc);
void comp_line(Session *S); /* the direct line in comp_cbuf, to comp_xbuf */
void comp_text(
    Session *S,
    const uint8_t *body
); /* the body at body to comp.buf; comp.len its length */
[[noreturn]]
void comp_fail(Session *S, uint8_t err); /* repl_error while comp.busy: ERR, EOL, unwind */
/* The statement's steps resolved, folded and generated, in order. */
void comp_flush(Session *S);
void comp_dfa(Session *S, uint16_t state); /* run the statement automaton from state */
uint8_t comp_expnames(uint16_t state, const char *names[]); /* what state wanted; PS_NC*2 room */
void comp_expected(Cons *co, uint16_t state);             /* print it, "a, b or c" */

/* Steps: the statement's nodes (comp/ast.h), each checked as made (comp/sema.h), generated in order
   at comp_flush */
typedef struct Node Node;
void comp_step(Session *S, Node *n);       /* tree n checked, the statement's next step */
void comp_check(Session *S, Node *n);      /* tree n checked, comp.p kept: a step's child */
uint8_t comp_type(Session *S);             /* the last step's value's T_* */
void comp_value(Session *S, uint8_t type); /* the last step leaves a value of type */
void comp_call(Session *S, uint8_t k);     /* CALL k */
void comp_int(Session *S, uint32_t n);     /* an INT's load */
Node *comp_op(Session *S, uint8_t op);     /* op; its operands after, by comp_byte, comp_word */
void comp_byte(Node *n, uint8_t b);
void comp_word(Node *n, uint16_t w);
/* op, a variable; a parameter's X.LD.V/X.ST.V as V.LD/V.ST */
void comp_opid(Session *S, uint8_t op, uint16_t id);
/* op (0 none), its operands by comp_byte, then comp.lnum's line: num addr */
Node *comp_jump(Session *S, uint8_t op);
void comp_go(Session *S, uint8_t op, Jump j);   /* op to a block's target */
void comp_cond(Session *S, uint8_t op, Jump j); /* JF or JT on the value: a number */

/* The last step's value converted; Type mismatch from a string where a number goes */
void comp_toint(Session *S);
void comp_tofloat(Session *S);
void comp_todouble(Session *S);
void comp_tostring(Session *S);
void comp_toinfo(Session *S, uint8_t info);
void comp_tobyte(Session *S);
void comp_toaddr(Session *S);
void comp_target(Session *S, uint16_t id); /* variable id is the target: comp.tgt, tgtinfo */
void comp_store(Session *S);               /* the value converted and stored to the target */

/* The token reader: p at a byte or the spaces before it */
uint8_t comp_peek(const uint8_t **p); /* st_get: *p at the byte past spaces; it, 0 the end */
uint8_t comp_next(const uint8_t **p); /* st_getn: comp_peek a byte on */
bool comp_eos(
    Session *S,
    const uint8_t **p
); /* st_eos: *p at 0, ':', ', or ELSE in a one-line IF */
static inline uint8_t comp_cur(Comp *const c) { return comp_peek(&c->p); }  /* st_get at comp.p */
static inline uint8_t comp_curn(Comp *const c) { return comp_next(&c->p); } /* st_getn */
bool comp_end(Session *S);                                                  /* st_eos at comp.p */
void comp_need(Session *S, uint8_t b); /* cm_chk: comp.p at b, stepped past; else Syntax error */
static inline void comp_open(Session *const S) { comp_need(S, '('); }
static inline void comp_close(Session *const S) { comp_need(S, ')'); }
static inline void comp_comma(Session *const S) { comp_need(S, ','); }
void comp_equals(Session *S);   /* cm_chkeq: comp_need(TK_EQ) */
bool comp_optcomma(Session *S); /* past a ',', if there */
uint8_t comp_lnum(Session *S);  /* a line number or label to lnum, jline; the byte after */

/* Names and variables */
uint16_t comp_name(Session *S); /* cm_name: the M_VAR at comp.p as comp_var(0, 0) */
uint16_t comp_var(
    Session *S,
    uint8_t shape,
    uint8_t et
);                                        /* var_id: shape 1 a scalar, 2 an array, 0 by '(';
                                            et its type, 0 by suffix, AS, DEFtype */
bool comp_ispar(Session *S, uint16_t id); /* a scalar parameter of the procedure compiled */
bool comp_param(Session *S, uint16_t name, uint8_t array, uint16_t *id); /* var_par: array 80h */
void comp_scope(Session *S, uint16_t name); /* var_scope: comp.scope, comp.isstatic for it */
uint8_t comp_suffix(uint8_t c);             /* et_sfx: % & ! # $ to ET_*, 0 none */
const uint8_t *comp_label(
    const uint8_t *body
);                                       /* lbl_at: a label starts body: its ':', else null */
uint16_t comp_nameof(const uint8_t *at); /* lbl_nam: the name of the M_VAR at at */
/* DATA's text from p (past the DATA token): to its ':' out of quotes, or its 0. */
const uint8_t *comp_skipdata(const uint8_t *p);
#endif
