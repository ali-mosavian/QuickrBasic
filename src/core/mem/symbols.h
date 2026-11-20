/* The name table's and the variables' interface. How it works: symbols.c. */
#ifndef MEM_SYMBOLS_H
#define MEM_SYMBOLS_H
#include "table.h"
#include "fault.h"
#include "mem/program.h"
#include "mem/loc.h"

/* A variable's info: its kind in bits 1-0 (comp's g_etk), SK_DBL, VI_ARR. */
enum : uint8_t { VK_FLT = 0, VK_INT = 1, VK_STR = 2, VK_MASK = 3, VI_ARR = 0x80 };
/* A hidden name's first byte: a FOR loop's limit or step (comp/control.c); the record a call's
   record result goes to (comp/refs.c). */
enum : uint8_t { LOOP_NAME = 1, RES_NAME = 2 };
/* A record array's info. */
enum : uint8_t { VI_RECARR = VI_ARR | VK_INT };

typedef struct PACKED {
    Addr vt;       /* its cell; an array's holds its ArrSlot */
    uint32_t type; /* ET_*, ET_REC + t a TYPE's */
    uint32_t name;
    uint8_t info; /* VI_ARR, SK_DBL, the kind */
    uint8_t pad[3];
} Var;            /* TVAR */
typedef struct PACKED {
    uint32_t next;  /* its hash chain's next */
    uint32_t nloc;  /* the next local of its procedure */
    uint32_t scope; /* 0 the main module, SC_MOD | n another, else a procedure */
    uint8_t given;  /* an array given elements by a line's code: link_given */
    uint8_t kop;    /* a CONST's literal load, its operand in symb_kval; 0 none (comp/fold.h) */
    uint8_t par;    /* a parameter of its procedure (link) */
    uint8_t base;   /* its file's OPTION BASE: an array's lower bound when not DIMmed */
} VarX;             /* TVX */
typedef struct PACKED {
    uint32_t text; /* its text's offset in text: its length, then its letters */
    uint32_t proc; /* its procedure, 0 none */
    uint32_t rec;  /* its TYPE, 0 none */
    uint8_t flags; /* NM_* */
    uint8_t pad[3];
} Name; /* TNAM */
static_assert(sizeof(Var) == 16 && sizeof(VarX) == 16 && sizeof(Name) == 16);

/* A local array's place in its frame (vm/flow.c): what its DIMs asked (dims << 24 | elements;
   FARR_NEVER: one not constant, or a REDIM), and where link put its header (from vm.fp; 0 the
   heap's). */
typedef struct {
    uint32_t shape;
    int32_t at;
} FrameArr;
enum : uint32_t { FARR_NEVER = 0xFFFFFFFF };

typedef struct Symb {
    Fault *fault;                        /* Out of memory */
    Table vars;                          /* the cells, Var, VarX, a CONST's literal (8 bytes) */
    Table names;                         /* Name, the chains' next (uint32_t) */
    Table text;                          /* the names' text */
    uint32_t text_free;                  /* text's next byte, an offset (sym.tfree) */
    uint32_t nvars;                      /* the next variable (lx.nsyms) */
    uint32_t nnames;                     /* the next name (lx.nnams) */
    Table locals;                        /* by procedure, its first local, VarX.nloc on (g_plh) */
    uint8_t deft[26];                    /* DEFtype's ET_*, by first letter (g_deft) */
    uint32_t vhead[HSIZE], nhead[HSIZE]; /* the hash chains' heads */
} Symb;

static inline Addr symb_cell(const Symb *const sy, const uint32_t id)
{
    return sy->vars.at[0] + id * (uint32_t)sizeof(Cell);
}
static inline Var *symb_var(const Symb *const sy, const uint32_t id)
{
    return (Var *)at(sy->vars.at[1]) + id;
}
/* Procedure proc's first local, 0 none. */
static inline uint32_t symb_first_local(const Symb *const sy, const uint32_t proc)
{
    return proc < sy->locals.cap ? ((const uint32_t *)at(sy->locals.at[0]))[proc] : 0;
}
static inline VarX *symb_varx(const Symb *const sy, const uint32_t id)
{
    return (VarX *)at(sy->vars.at[2]) + id;
}
/* Where variable id's cell is: a local's in its frame, a parameter's through its ref cell
   (link_procvars); 0 its own cell, symb_cell. */
static inline Loc *symb_loc(const Symb *const sy, const uint32_t id)
{
    return (Loc *)at(sy->vars.at[4]) + id;
}
/* symb_loc with its own cell's address: an op's operand (link). */
static inline Loc symb_placed(const Symb *const sy, const uint32_t id)
{
    const Loc p = *symb_loc(sy, id);
    return p.w ? p : loc_cell(symb_cell(sy, id));
}
/* Variable id's cell, in the frame at fp. */
static inline Addr symb_at(const Symb *const sy, const Addr fp, const uint32_t id)
{
    const Loc p = *symb_loc(sy, id);
    return p.w ? loc_frame_at(p, fp) : symb_cell(sy, id);
}
/* In its procedure's frame: a local's cell, or a parameter's ref cell. */
static inline bool symb_in_frame(const Symb *const sy, const uint32_t id)
{
    return loc_in_frame(*symb_loc(sy, id));
}
static inline bool symb_is_ref(const Symb *const sy, const uint32_t id)
{
    return loc_is_ref(*symb_loc(sy, id));
}
static inline bool symb_is_local(const Symb *const sy, const uint32_t id)
{
    return loc_is_local(*symb_loc(sy, id));
}
/* A record, not an array of them. */
static inline bool symb_is_record(const Symb *const sy, const uint32_t id)
{
    const Var *const v = symb_var(sy, id);
    return v->type >= ET_REC && (v->info & (VI_ARR | VK_MASK)) == VI_RECARR;
}
static inline FrameArr *symb_farr(const Symb *const sy, const uint32_t id)
{
    return (FrameArr *)at(sy->vars.at[5]) + id;
}
static inline uint8_t *symb_kval(const Symb *const sy, const uint32_t id)
{
    return at(sy->vars.at[3]) + id * 8u;
}
static inline Name *symb_name(const Symb *const sy, uint32_t id)
{
    return (Name *)at(sy->names.at[0]) + id;
}
/* Name id's text: its length, then its letters in capitals, then as first written. */
static inline const uint8_t *symb_text(const Symb *const sy, uint32_t id)
{
    return at(sy->text.at[0] + symb_name(sy, id)->text);
}
/* dst's n characters, name id's in any casing and maybe a suffix after: its letters as first
   written. */
static inline void symb_spell(const Symb *const sy, uint32_t id, uint8_t *const dst, uint8_t n)
{
    const uint8_t *const t = symb_text(sy, id);
    memcpy(dst, t + 1 + t[0], t[0] < n ? t[0] : n);
}

/* The tables' blocks from al, after the program store's. */
void symb_new(Symb *sy, Allo *al, Fault *f);
/* Room for a program's variables before it runs, nnames its M_VAR tokens: they are made lazily,
   when the heap may hold strings and arrays and the block cannot grow. */
void symb_size(Symb *sy, uint32_t nnames);
/* Every variable's value gone: the cells zeroed, an array's header none, vt its own cell
   (session_clear's part). */
void symb_clear(Symb *sy);
/* No names, no variables. */
void symb_reset(Symb *sy);
/* No variables; the names kept. */
void symb_vars_init(Symb *sy);
/* The name of text[0..len) (no suffix), made when new (NM_FN when it starts FN). Out of memory:
   past SC_LBL names, or the heap not empty. */
uint32_t symb_find_name(Symb *sy, const uint8_t *text, uint8_t len);
/* The variable of name, type (ET_*) and kind (VI_ARR, the low bits as info's) in scope (a module's
   or a procedure's), made when new, a scalar zeroed; a new one of a procedure joins its locals
   unless stat. Out of memory: the heap not empty. */
uint32_t symb_find_var(
    Symb *sy,
    uint32_t name,
    uint32_t type,
    uint8_t kind,
    uint32_t scope,
    bool stat
);
#endif
