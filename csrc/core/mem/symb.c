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
};

static uint16_t vhead[HSIZE], nhead[HSIZE];

static uint16_t *name_next(void) { return (uint16_t *)at(symb.names.at[1]); }

static uint8_t upcase(uint8_t c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }

/* The variables' block moved or grew: the scalars' vt by the cells' move. */
static void var_place(void)
{
    Addr old = tabl_place(&symb.vars);
    uint32_t delta = symb.vars.base - old;
    if (!delta)
        return;
    for (uint16_t id = 1; id < symb.nvars; id++) {
        Var *v = symb_var(id);
        if (!(v->info & VI_ARR) && v->vt)
            v->vt += delta;
    }
}

static void nam_place(void) { tabl_place(&symb.names); }
static void txt_place(void) { tabl_place(&symb.text); }

void symb_new(void)
{
    Table *t = &symb.vars;
    t->narr = 3;
    t->size[0] = t->size[1] = t->size[2] = 8; /* the cells, Var, VarX */
    t->max = IDMAX;
    tabl_new(t, VINIT, var_place);
    var_place();
    t = &symb.names;
    t->narr = 2;
    t->size[0] = 8; /* Name, the chains */
    t->size[1] = 2;
    t->max = IDMAX;
    tabl_new(t, NINIT, nam_place);
    t = &symb.text;
    t->narr = 1;
    t->size[0] = 1;
    t->max = IDMAX * 256;
    tabl_new(t, NINIT * 8, txt_place);
}

void symb_size(void)
{
    uint32_t n = symb.nvars + VSLACK;
    for (uint32_t l = PROGBASE; prog_line(l)->len; l += prog_line(l)->len) {
        const uint8_t *p = at(prog_addr(l));
        for (uint32_t i = HDR; i < prog_line(l)->len; i++)
            n += p[i] == M_VAR;
    }
    if (n > IDMAX)
        n = IDMAX;
    tabl_room(&symb.vars, n); /* no room: the variable past it errs */
}

void symb_clear(void)
{
    memset(at(symb.vars.at[0]), 0, symb.vars.cap * 8);
    for (uint16_t id = 0; id < symb.nvars; id++) {
        Var *v = symb_var(id);
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
    exec.lsp = exec.lsav;
    stmt_restore();
}

void symb_init(void)
{
    symb.nnames = 1;
    symb.text_free = 0;
    memset(nhead, 0, sizeof nhead);
    symb_vars_init();
    decl_reset();
}

void symb_vars_init(void)
{
    symb.nvars = 1;
    memset(vhead, 0, sizeof vhead);
    memset(symb.first_local, 0, sizeof symb.first_local);
}

uint16_t symb_find_name(const uint8_t *text, uint8_t len)
{
    uint32_t h = 0;
    for (uint8_t i = 0; i < len; i++)
        h = h * 31 + upcase(text[i]);
    h &= HSIZE - 1;
    for (uint16_t id = nhead[h]; id; id = name_next()[id]) {
        const uint8_t *t = symb_text(id);
        if (t[0] != len)
            continue;
        uint8_t i = 0;
        while (i < len && upcase(text[i]) == t[1 + i])
            i++;
        if (i == len)
            return id;
    }
    uint16_t id = symb.nnames;
    if (!tabl_room(&symb.names, id + 1u) || !tabl_room(&symb.text, len + symb.text_free + 1))
        repl_error(E_OM);
    Name *n = symb_name(id);
    *n = (Name){.text = symb.text_free};
    uint8_t *d = at(symb.text.at[0] + symb.text_free);
    d[0] = len;
    for (uint8_t i = 0; i < len; i++)
        d[1 + i] = upcase(text[i]);
    symb.text_free += len + 1u;
    name_next()[id] = nhead[h];
    nhead[h] = id;
    if (len > 2 && d[1] == 'F' && d[2] == 'N')
        n->flags = NM_FN;
    symb.nnames++;
    return id;
}

uint16_t symb_find_var(uint16_t name, uint8_t type, uint8_t kind, uint8_t scope, bool stat)
{
    uint32_t h = (name * 5u + type * 2u + (kind >> 7) + scope * 8u) & (HSIZE - 1);
    for (uint16_t id = vhead[h]; id; id = symb_varx(id)->next) {
        const Var *v = symb_var(id);
        if (v->name == name && v->type == type && (v->info & (VI_ARR | SK_DBL | VK_MASK)) == kind &&
            symb_varx(id)->scope == scope)
            return id;
    }
    uint16_t id = symb.nvars;
    if (id >= symb.vars.cap && !tabl_room(&symb.vars, id + 1u))
        repl_error(E_OM);
    Var *v = symb_var(id);
    VarX *x = symb_varx(id);
    *v = (Var){.name = name, .type = type, .info = kind};
    *x = (VarX){.next = vhead[h], .scope = scope};
    vhead[h] = id;
    if (scope && !stat) { /* a local: on its procedure's list */
        x->nloc = symb.first_local[scope];
        symb.first_local[scope] = id;
    }
    if (!(kind & VI_ARR)) {
        v->vt = symb_cell(id);
        wr32(v->vt, 0);
        wr32(v->vt + 4, 0);
    }
    symb.nvars++;
    return id;
}
