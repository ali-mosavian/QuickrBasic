/* The compiler (comp/comp.asm): a token body to stack-machine code. A statement is run by the
   generated automaton (comp/dfa.h): each state's entry for the byte at the cursor steps past a
   token, goes on at the statement's end, or runs an action, an emitter below (A_*) or CALL K_*.

   The token cursor is comp.p, the code's next byte comp.out. A body is in the arena (comp_cbuf or
   the program store), so a string literal's operand is addr_of() its text.

   Errors: while comp.busy, repl_error(code) calls comp_fail(code), which ends the line's code with
   ERR code dg at state, EOL and longjmps to comp_text's setjmp; comp_text then returns normally.
   The error is raised when the code runs. */
#ifndef COMP_COMP_H
#define COMP_COMP_H
#include <setjmp.h>
#include "defs.h"

/* The compiled expression's target (comp.tgtsubs): FFh a record's field (ctrl.field). */
enum : uint8_t { TGT_FIELD = 0xFF };

enum { COMP_TAIL = 12 }; /* room past the code's limit: a line's ERR and EOL */

typedef struct {
    /* the cursor and the code */
    const uint8_t *p;     /* (ESI) the token cursor */
    uint8_t *out;         /* (EDI) where the next byte goes */
    uint8_t *buf;         /* cxb: the code's start, comp_xbuf */
    uint8_t *lim;         /* cxlim: the code's limit, below the map */
    uint8_t *map;         /* cmap: the statement map's top; its Stmt entries grow down */
    uint32_t nstmt;       /* statements recorded */
    const uint8_t *body;  /* cbase: the body's start */
    uint32_t line;        /* the line's offset in the program store, DLINE direct */
    uint8_t len;          /* xo: the code's length, mod 256 */
    bool busy;            /* csp: compiling; an error is comp_fail's */
    jmp_buf unwind;       /* csp: comp_fail's way back to comp_text */
    uint16_t depth;       /* cplim: expression nesting, Out of memory past COMP_MAXDEPTH */

    /* the expression */
    uint8_t type;         /* cty: its T_* */
    uint8_t *iload;       /* cll: the last INT load emitted, its end */
    uint8_t *iloadend;    /* clle */
    uint8_t *cmp;         /* ccmp: the last compare, its end (a one-line IF's CMPIF) */
    uint8_t *cmpend;      /* ccmpe */
    uint8_t *vat;         /* g_cvat: a store's value's code, its start (expr_target's end) */
    bool folding;         /* g_cfold: fold_code's run; an error is its */
    jmp_buf fold;         /* g_cfsp: fold_code's way back from an error */

    /* the target of a store */
    uint16_t tgt;         /* ctv: its variable, or a record field's offset */
    uint8_t tgtinfo;      /* ctk: its TVAR.info */
    uint8_t tgtsubs;      /* ctn: its subscripts, TGT_FIELD a record's field */

    /* the statement */
    const uint8_t *kw;    /* ctks: its keyword */
    uint8_t tok;          /* ctok: that token (LOAD, SAVE, VERIFY) */
    bool then;            /* cth: it follows a THEN */
    bool oneif;           /* csif: in a one-line IF: ELSE ends a statement */
    uint8_t *dbg;         /* cdbg: past its DBG op, null none */
    uint8_t nargs;        /* ccnt: a CALL's counted arguments */
    uint8_t forkind;      /* cfk: FOR's variable's kind, 0 a FLOAT */
    uint16_t forvar;      /* cfid: FOR's variable */
    uint8_t *onat;        /* onp: ON's count byte */
    uint16_t redim;       /* credim: DIM_REDIM, DIM_PRESERVE or 0 */
    uint16_t lnum;        /* wv: a jump's line number, NONUM a label's */
    uint32_t jline;       /* jline: its line, 0 until the first jump finds it */
    bool modlabel;        /* g_lmod: comp_lnum's label is the module's (cleared) */

    /* scopes */
    uint8_t proc;         /* cproc: the procedure the line is in, 0 the module (mem/scope sets it) */
    uint8_t scope;        /* cscope: var_find's scope: 0 the module, a procedure */
    bool isstatic;        /* cstat: var_find makes a STATIC, not a local */
    uint8_t dlproc;       /* g_dlprc: the direct line's procedure (the debugger's probe) */
    bool check;           /* cchk: the IDE's check of a line alone, unlinked */

    /* the error */
    uint8_t dg;           /* cdg: its DG_ (io), 0 by its text */
    uint16_t dfa;         /* cdfa: the automaton's state it failed in, FFFFh none */

    uint64_t dmask;       /* dimsubs: bit i, subscript i its hi only */
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
void comp_init(Session *S);                    /* comp_cbuf carved (allo_carve) */
void comp_line(Session *S);                    /* the direct line in comp_cbuf, to comp_xbuf */
void comp_text(Session *S, const uint8_t *body);     /* the body at body to comp.buf; comp.len its length */
[[noreturn]] void comp_fail(Session *S, uint8_t err); /* repl_error while comp.busy: ERR, EOL, unwind */
void comp_dfa(Session *S, uint16_t state);           /* run the statement automaton from state */
uint8_t comp_expnames(uint16_t state, const char *names[]); /* what state wanted; PS_NC*2 room */
void comp_expected(Session *S, uint16_t state);      /* print it, "a, b or c" */

/* Emitters */
void comp_byte(Session *S, uint8_t b);               /* Out of memory past comp.lim */
static inline void comp_op(Session *const S, uint8_t op) { comp_byte(S, op); }
void comp_opb(Session *S, uint8_t op, uint8_t b);    /* op, a byte */
void comp_opw(Session *S, uint8_t op, uint16_t w);   /* op, a word */
void comp_opid(Session *S, uint8_t op, uint16_t id); /* op, a variable; a parameter's X.LD.V/X.ST.V as V.LD/V.ST */
void comp_word(Session *S, uint16_t w);
void comp_dword(Session *S, uint32_t d);
void comp_insert(Session *S, uint8_t b, uint8_t *at); /* b at at, the code after it moved up */
void comp_int(Session *S, uint32_t n);               /* an INT load: I.LD.U8, I.LD.I16 or I.LD.I32 */
void comp_call(Session *S, uint8_t k);               /* CALL k */
void comp_lref(Session *S);                    /* a TLREF: comp.lnum, comp.jline */
uint8_t *comp_intleaf(Session *S);             /* the code's last op if an INT load, else null */

/* Type's one of four: INT, FLOAT, DOUBLE, STRING. */
static inline uint8_t comp_pick(const uint8_t type, const uint8_t i, const uint8_t f, const uint8_t d,
                                const uint8_t s)
{
    switch (type) {
    case T_INT: return i;
    case T_FLT: return f;
    case T_DBL: return d;
    default: return s;
    }
}

/* The value's type (comp.type); Type mismatch from a string where a number goes */
void comp_toint(Session *S);
void comp_tofloat(Session *S);                 /* an INT load just made loads a FLOAT instead */
void comp_todouble(Session *S);
void comp_tostring(Session *S);
void comp_toinfo(Session *S, uint8_t info);          /* as a variable of TVAR.info info holds it */
void comp_tobyte(Session *S);
void comp_toaddr(Session *S);
void comp_dtest(Session *S);                   /* cm_dnz: a DOUBLE test as an INT, D.NZ */
void comp_target(Session *S, uint16_t id);           /* variable id is the target: comp.tgt, tgtinfo */
void comp_store(Session *S);                   /* the value converted and stored to the target */

/* The token reader: p at a byte or the spaces before it */
uint8_t comp_peek(const uint8_t **p);    /* st_get: *p at the byte past spaces; it, 0 the end */
uint8_t comp_next(const uint8_t **p);    /* st_getn: comp_peek a byte on */
bool comp_eos(Session *S, const uint8_t **p);        /* st_eos: *p at 0, ':', ', or ELSE in a one-line IF */
static inline uint8_t comp_cur(Comp *const c) { return comp_peek(&c->p); }  /* st_get at comp.p */
static inline uint8_t comp_curn(Comp *const c) { return comp_next(&c->p); } /* st_getn */
bool comp_end(Session *S); /* st_eos at comp.p */
void comp_need(Session *S, uint8_t b);               /* cm_chk: comp.p at b, stepped past; else Syntax error */
static inline void comp_open(Session *const S) { comp_need(S, '('); }
static inline void comp_close(Session *const S) { comp_need(S, ')'); }
static inline void comp_comma(Session *const S) { comp_need(S, ','); }
void comp_equals(Session *S);                  /* cm_chkeq: comp_need(TK_EQ) */
bool comp_optcomma(Session *S);                /* past a ',', if there */
uint8_t comp_lnum(Session *S);                 /* a line number or label to lnum, jline; the byte after */

/* Names and variables */
uint16_t comp_name(Session *S);                /* cm_name: the M_VAR at comp.p as comp_var(0, 0) */
uint16_t comp_var(Session *S, uint8_t shape, uint8_t et); /* var_id: shape 1 a scalar, 2 an array, 0 by '(';
                                            et its type, 0 by suffix, AS, DEFtype */
bool comp_ispar(Session *S, uint16_t id);            /* a scalar parameter of the procedure compiled */
bool comp_param(Session *S, uint16_t name, uint8_t array, uint16_t *id); /* var_par: array 80h */
void comp_scope(Session *S, uint16_t name);          /* var_scope: comp.scope, comp.isstatic for it */
uint8_t comp_suffix(uint8_t c);          /* et_sfx: % & ! # $ to ET_*, 0 none */
const uint8_t *comp_label(const uint8_t *body); /* lbl_at: a label starts body: its ':', else null */
uint16_t comp_nameof(const uint8_t *at); /* lbl_nam: the name of the M_VAR at at */
/* DATA's text from p (past the DATA token): to its ':' out of quotes, or its 0. */
const uint8_t *comp_skipdata(const uint8_t *p);
#endif
