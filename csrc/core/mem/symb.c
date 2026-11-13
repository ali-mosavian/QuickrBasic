#include "mem/symb.h"
#include "comp/decl.h"
#include "io/repl.h"
#include "mem/arry.h"
#include "mem/prog.h"
#include "mem/strg.h"
#include "vm/exec.h"
#include "vm/stmt.h"

Symb symb;

enum {
    VINIT = 1024,  /* the first variables' room */
    NINIT = 1024,  /*   names' */
    IDMAX = 0xFFFF,
    VSLACK = 256,  /* variables past the program's: direct lines' */
    NAME_HASH_MUL = 31,
};


static uint16_t *name_next(void) { return (uint16_t *)at(symb.names.at[1]); }

/* The variables' block moved or grew: the scalars' vt by the cells' move. */
static void var_place(void)
{
    const Addr old = tabl_place(&symb.vars);
    const uint32_t delta = symb.vars.base - old;
    if (!delta)
        return;
    for (uint16_t id = 1; id < symb.nvars; id++) {
        Var *const v = symb_var(id);
        if (!(v->info & VI_ARR) && v->vt)
            v->vt += delta;
    }
}

static void nam_place(void) { tabl_place(&symb.names); }
static void txt_place(void) { tabl_place(&symb.text); }

void symb_new(void)
{
    symb.vars = (Table){.narr = 3, .size = {sizeof(Cell), sizeof(Var), sizeof(VarX)}, .max = IDMAX};
    tabl_new(&symb.vars, VINIT, var_place);
    var_place();
    symb.names = (Table){.narr = 2, .size = {sizeof(Name), sizeof(uint16_t)}, .max = IDMAX};
    tabl_new(&symb.names, NINIT, nam_place);
    symb.text = (Table){.narr = 1, .size = {1}, .max = IDMAX * 256};
    tabl_new(&symb.text, NINIT * 8, txt_place);
}

void symb_size(void)
{
    uint32_t n = symb.nvars + VSLACK;
    for (uint32_t l = PROGBASE; prog_line(l)->len; l += prog_line(l)->len) {
        const uint8_t *const p = at(prog_addr(l));
        for (uint32_t i = HDR; i < prog_line(l)->len; i++)
            n += p[i] == M_VAR;
    }
    if (n > IDMAX)
        n = IDMAX;
    tabl_room(&symb.vars, n); /* no room: the variable past it errs */
}

void symb_clear(void)
{
    memset(at(symb.vars.at[0]), 0, symb.vars.cap * sizeof(Cell));
    for (uint16_t id = 0; id < symb.nvars; id++) {
        Var *const v = symb_var(id);
        if (v->info & VI_ARR) {
            v->vt = 0;
            symb_varx(id)->dyn = 0;
        } else {
            v->vt = symb_cell(id); /* a parameter's, bound elsewhere: back */
        }
    }
    arry.end = allo.bot;
    strg.low = allo.top;
    arry.hdr = allo.hdr_base;
    strg.tmp_used = 0;
    strg.tmp_floor = 0;
    vm.ncallvars = 0;
    stmt_restore();
}

void symb_init(void)
{
    symb.nnames = 1;
    symb.text_free = 0;
    memset(symb.nhead, 0, sizeof symb.nhead);
    symb_vars_init();
    decl_reset();
}

void symb_vars_init(void)
{
    symb.nvars = 1;
    memset(symb.vhead, 0, sizeof symb.vhead);
    memset(symb.first_local, 0, sizeof symb.first_local);
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

uint16_t symb_find_name(const uint8_t *const text, const uint8_t len)
{
    uint32_t hash = 0;
    for (uint8_t i = 0; i < len; i++)
        hash = hash * NAME_HASH_MUL + upcase(text[i]);
    const uint32_t h = hash & (HSIZE - 1);
    for (uint16_t id = symb.nhead[h]; id; id = name_next()[id])
        if (spells(symb_text(id), text, len))
            return id;
    const uint16_t id = symb.nnames;
    if (!tabl_room(&symb.names, id + 1u) || !tabl_room(&symb.text, len + symb.text_free + 1))
        repl_error(E_OM);
    Name *const n = symb_name(id);
    *n = (Name){.text = symb.text_free};
    uint8_t *const d = at(symb.text.at[0] + symb.text_free);
    d[0] = len;
    for (uint8_t i = 0; i < len; i++)
        d[1 + i] = upcase(text[i]);
    symb.text_free += len + 1u;
    name_next()[id] = symb.nhead[h];
    symb.nhead[h] = id;
    if (len > 2 && d[1] == 'F' && d[2] == 'N')
        n->flags = NM_FN;
    symb.nnames++;
    return id;
}

uint16_t symb_find_var(const uint16_t name, const uint8_t type, const uint8_t kind,
                       const uint8_t scope, const bool stat)
{
    const uint32_t h = (name * 5u + type * 2u + (kind >> 7) + scope * 8u) & (HSIZE - 1);
    for (uint16_t id = symb.vhead[h]; id; id = symb_varx(id)->next) {
        const Var *const v = symb_var(id);
        if (v->name == name && v->type == type && (v->info & (VI_ARR | SK_DBL | VK_MASK)) == kind &&
            symb_varx(id)->scope == scope)
            return id;
    }
    const uint16_t id = symb.nvars;
    if (id >= symb.vars.cap && !tabl_room(&symb.vars, id + 1u))
        repl_error(E_OM);
    Var *const v = symb_var(id);
    VarX *const x = symb_varx(id);
    *v = (Var){.name = name, .type = type, .info = kind};
    *x = (VarX){.next = symb.vhead[h], .scope = scope};
    symb.vhead[h] = id;
    if (scope && !stat) { /* a local: on its procedure's list */
        x->nloc = symb.first_local[scope];
        symb.first_local[scope] = id;
    }
    if (!(kind & VI_ARR)) {
        v->vt = symb_cell(id);
        memset(at(v->vt), 0, sizeof(Cell));
    }
    symb.nvars++;
    return id;
}
