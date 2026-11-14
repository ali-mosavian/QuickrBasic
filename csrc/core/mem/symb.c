#include "mem/symb.h"
#include "session.h"
#include "comp/decl.h"
#include "io/repl.h"
#include "mem/arry.h"
#include "mem/prog.h"
#include "mem/strg.h"
#include "vm/exec.h"
#include "vm/stmt.h"


enum {
    VINIT = 1024,  /* the first variables' room */
    NINIT = 1024,  /*   names' */
    IDMAX = 0xFFFF,
    VSLACK = 256,  /* variables past the program's: direct lines' */
    NAME_HASH_MUL = 31,
};


static uint16_t *name_next(Session *const S) { return (uint16_t *)at(S->symb.names.at[1]); }

/* The variables' block moved or grew: the scalars' vt by the cells' move. */
static void var_place(Session *const S)
{
    const Addr old = tabl_place(S, &S->symb.vars);
    const uint32_t delta = S->symb.vars.base - old;
    if (!delta)
        return;
    for (uint16_t id = 1; id < S->symb.nvars; id++) {
        Var *const v = symb_var(&S->symb, id);
        if (!(v->info & VI_ARR) && v->vt)
            v->vt += delta;
    }
}

static void nam_place(Session *const S) { tabl_place(S, &S->symb.names); }
static void txt_place(Session *const S) { tabl_place(S, &S->symb.text); }

void symb_new(Session *const S)
{
    S->symb.vars = (Table){.narr = 3, .size = {sizeof(Cell), sizeof(Var), sizeof(VarX)}, .max = IDMAX};
    tabl_new(S, &S->symb.vars, VINIT, var_place);
    var_place(S);
    S->symb.names = (Table){.narr = 2, .size = {sizeof(Name), sizeof(uint16_t)}, .max = IDMAX};
    tabl_new(S, &S->symb.names, NINIT, nam_place);
    S->symb.text = (Table){.narr = 1, .size = {1}, .max = IDMAX * 256};
    tabl_new(S, &S->symb.text, NINIT * 8, txt_place);
}

void symb_size(Session *const S)
{
    uint32_t n = S->symb.nvars + VSLACK;
    for (uint32_t l = PROGBASE; prog_line(&S->prog, l)->len; l += prog_line(&S->prog, l)->len) {
        const uint8_t *const p = at(prog_addr(&S->prog, l));
        for (uint32_t i = HDR; i < prog_line(&S->prog, l)->len; i++)
            n += p[i] == M_VAR;
    }
    if (n > IDMAX)
        n = IDMAX;
    tabl_room(S, &S->symb.vars, n); /* no room: the variable past it errs */
}

void symb_clear(Session *const S)
{
    memset(at(S->symb.vars.at[0]), 0, S->symb.vars.cap * sizeof(Cell));
    for (uint16_t id = 0; id < S->symb.nvars; id++) {
        Var *const v = symb_var(&S->symb, id);
        if (v->info & VI_ARR) {
            v->vt = 0;
            symb_varx(&S->symb, id)->dyn = 0;
        } else {
            v->vt = symb_cell(&S->symb, id); /* a parameter's, bound elsewhere: back */
        }
    }
    S->arry.end = S->allo.bot;
    S->strg.low = S->allo.top;
    S->arry.hdr = S->allo.hdr_base;
    S->strg.tmp_used = 0;
    S->strg.tmp_floor = 0;
    S->vm.ncallvars = 0;
    stmt_restore(S);
}

void symb_init(Session *const S)
{
    S->symb.nnames = 1;
    S->symb.text_free = 0;
    memset(S->symb.nhead, 0, sizeof S->symb.nhead);
    symb_vars_init(S);
    decl_reset(S);
}

void symb_vars_init(Session *const S)
{
    S->symb.nvars = 1;
    memset(S->symb.vhead, 0, sizeof S->symb.vhead);
    memset(S->symb.first_local, 0, sizeof S->symb.first_local);
}

/* Do the len chars at text, case not told, spell name text t (its length, then upper case)? */
static bool spells(const uint8_t *const t, const uint8_t *const text, const uint8_t len)
{
    if (t[0] != len)
        return false;
    for (uint8_t i = 0; i < len; i++)
        if (upcase(text[i]) != t[1 + i])
            return false;
    return true;
}

uint16_t symb_find_name(Session *const S, const uint8_t *const text, const uint8_t len)
{
    uint32_t hash = 0;
    for (uint8_t i = 0; i < len; i++)
        hash = hash * NAME_HASH_MUL + upcase(text[i]);
    const uint32_t h = hash & (HSIZE - 1);
    for (uint16_t id = S->symb.nhead[h]; id; id = name_next(S)[id])
        if (spells(symb_text(&S->symb, id), text, len))
            return id;
    const uint16_t id = S->symb.nnames;
    if (!tabl_room(S, &S->symb.names, id + 1u) || !tabl_room(S, &S->symb.text, len + S->symb.text_free + 1))
        repl_error(S, E_OM);
    Name *const n = symb_name(&S->symb, id);
    *n = (Name){.text = S->symb.text_free};
    uint8_t *const d = at(S->symb.text.at[0] + S->symb.text_free);
    d[0] = len;
    for (uint8_t i = 0; i < len; i++)
        d[1 + i] = upcase(text[i]);
    S->symb.text_free += len + 1u;
    name_next(S)[id] = S->symb.nhead[h];
    S->symb.nhead[h] = id;
    if (len > 2 && d[1] == 'F' && d[2] == 'N')
        n->flags = NM_FN;
    S->symb.nnames++;
    return id;
}

uint16_t symb_find_var(Session *const S, const uint16_t name, const uint8_t type, const uint8_t kind,
                       const uint8_t scope, const bool stat)
{
    const uint32_t h = (name * 5u + type * 2u + (kind >> 7) + scope * 8u) & (HSIZE - 1);
    for (uint16_t id = S->symb.vhead[h]; id; id = symb_varx(&S->symb, id)->next) {
        const Var *const v = symb_var(&S->symb, id);
        if (v->name == name && v->type == type && (v->info & (VI_ARR | SK_DBL | VK_MASK)) == kind &&
            symb_varx(&S->symb, id)->scope == scope)
            return id;
    }
    const uint16_t id = S->symb.nvars;
    if (id >= S->symb.vars.cap && !tabl_room(S, &S->symb.vars, id + 1u))
        repl_error(S, E_OM);
    Var *const v = symb_var(&S->symb, id);
    VarX *const x = symb_varx(&S->symb, id);
    *v = (Var){.name = name, .type = type, .info = kind};
    *x = (VarX){.next = S->symb.vhead[h], .scope = scope};
    S->symb.vhead[h] = id;
    if (scope && !stat) { /* a local: on its procedure's list */
        x->nloc = S->symb.first_local[scope];
        S->symb.first_local[scope] = id;
    }
    if (!(kind & VI_ARR)) {
        v->vt = symb_cell(&S->symb, id);
        memset(at(v->vt), 0, sizeof(Cell));
    }
    S->symb.nvars++;
    return id;
}
