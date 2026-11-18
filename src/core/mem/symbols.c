/* The name table and the variables.
 *
 * # How it works
 * A name, 1 .. nnames-1, as QBasic's name table: a Name in names, its text in text, its length then
 * its letters in upper case, a suffix left out. A variable, 1 .. nvars-1, as QBasic's variable
 * manager: a name, a type (ET_*) and whether an array.
 *
 * vars holds per variable its 8-byte cell, a Var and a VarX. Var.vt is its cell; an array's holds
 * its ArrSlot. A local's and a parameter's cells are in their call's frame (vm/flow.h). Linked code holds cells'
 * Addrs: the cells move only when a block before them grows, the cache while linking (before
 * link_resolve) or the program store on an edit, which drops the code. Each table chains its hashes from a head,
 * 0 ends a chain.
 *
 * # asm structure (retired/asm32/mem/sym.asm)
 * | asm | here | |
 * |---|---|---|
 * | TVAR (vt nam typ info) | Var (.vt .name .type .info) | MIRROR |
 * | TVX (next nloc scp dyn) | VarX (.next .nloc .scope .pad) | DIVERGES: dyn is the array's cell's |
 * | TNAM (text flag typ prc) | Name (.text .flags .rec .proc) | MIRROR |
 * | TSYM, g_state.sym | Symb | MIRROR |
 * | sym.vars, sym.nams, sym.ntext | Symb.vars, Symb.names, Symb.text | MIRROR |
 * | sym.tfree | Symb.text_free | MIRROR |
 * | lx.nsyms, lx.nnams | Symb.nvars, Symb.nnames | MOVED: declared here, not by the lexer |
 * | g_plh, g_deft | Symb.first_local[], Symb.deft[] | MIRROR |
 * | g_vhead, g_nhead | Symb.vhead, Symb.nhead | MIRROR |
 * | fs:[id*8].TVAR, vxat, nmat, cellof | symb_var, symb_varx, symb_name, symb_cell | MIRROR; D1 |
 * | nmtest id, bits | symb_name(id)->flags & bits | MIRROR |
 * | [ntext.at + TNAM.text] | symb_text(id) | MIRROR |
 * | sym_new, var_size, var_clr | symb_new, symb_size, symb_clear (session_clear the rest) | MIRROR |
 * | sym_init, var_init | symb_reset, symb_vars_init | MIRROR |
 * | nam_find (nptr, nlen -> EAX) | symb_find_name(text, len) | MIRROR; D2 |
 * | var_find (AX, CL, DL, cscope, cstat -> EAX) | symb_find_var(name, type, kind, scope, stat) | MIRROR; D2 |
 * | var_cell (EBX -> ECX) | symb_cell(id) | MIRROR |
 * | var_place, nam_place, txt_place | var_place, nam_place, txt_place | MIRROR; static |
 * | VINIT NINIT IDMAX VSLACK | same | MIRROR; static |
 * | sym.vsel | | D1 |
 *
 * - D1. No selector: an id indexes a table by Addr.
 * - D2. The asm's globals are parameters: nptr and nlen are text and len; cm.cscope and cm.cstat
 *   are scope and stat.
 */
#include "mem/symbols.h"

enum {
    VINIT = 1024, /* the first variables' room */
    NINIT = 1024, /*   names' */
    IDMAX = 0xFFFF,
    VSLACK = 256, /* variables past the program's: direct lines' */
    NAME_HASH_MUL = 31,
};

static uint16_t *symb_name_next(const Symb *const sy) { return (uint16_t *)at(sy->names.at[1]); }

/* The variables' block moved or grew: vt by the cells' move. */
static void symb_vars_moved(Table *const t, const Addr old)
{
    Symb *const sy = container_of(t, Symb, vars);
    const uint32_t delta = t->base - old;

    if (!delta)
        return;
    for (uint16_t id = 1; id < sy->nvars; id++) {
        Var *const v = symb_var(sy, id);

        if (v->vt)
            v->vt += delta;
    }
}

void symb_new(Symb *const sy, Allo *const al, Fault *const f)
{
    sy->fault = f;
    sy->vars = (Table){
        .narr = 6,
        .size = {sizeof(Cell), sizeof(Var), sizeof(VarX), 8, sizeof(Loc), sizeof(FrameArr)},
        .max = IDMAX,
        .moved = symb_vars_moved,
    };

    tabl_new(al, &sy->vars, VINIT);
    sy->names = (Table){.narr = 2, .size = {sizeof(Name), sizeof(uint16_t)}, .max = IDMAX};

    tabl_new(al, &sy->names, NINIT);
    sy->text = (Table){.narr = 1, .size = {1}, .max = IDMAX * 256};

    tabl_new(al, &sy->text, NINIT * 8);
}

void symb_size(Symb *const sy, const uint32_t nnames)
{
    uint32_t n = sy->nvars + VSLACK + nnames;

    if (n > IDMAX)
        n = IDMAX;

    tabl_room(&sy->vars, n); /* no room: the variable past it errs */
}

void symb_clear(Symb *const sy)
{
    memset(at(sy->vars.at[0]), 0, sy->vars.cap * sizeof(Cell));

    for (uint16_t id = 0; id < sy->nvars; id++) {
        symb_var(sy, id)->vt = symb_cell(sy, id);
    }
}

void symb_reset(Symb *const sy)
{
    sy->nnames = 1;
    sy->text_free = 0;
    memset(sy->nhead, 0, sizeof sy->nhead);
    symb_vars_init(sy);
}

void symb_vars_init(Symb *const sy)
{
    sy->nvars = 1;
    memset(sy->vhead, 0, sizeof sy->vhead);
    memset(sy->first_local, 0, sizeof sy->first_local);
}

/* Do the len chars at text, case not told, spell name text t (its length, then upper case)? */
static bool symb_spells(const uint8_t *const t, const uint8_t *const text, const uint8_t len)
{
    if (t[0] != len)
        return false;
    for (uint8_t i = 0; i < len; i++)
        if (upcase(text[i]) != t[1 + i])
            return false;

    return true;
}

uint16_t symb_find_name(Symb *const sy, const uint8_t *const text, const uint8_t len)
{
    uint32_t hash = 0;

    for (uint8_t i = 0; i < len; i++)
        hash = hash * NAME_HASH_MUL + upcase(text[i]);

    const uint32_t h = hash & (HSIZE - 1);

    for (uint16_t id = sy->nhead[h]; id; id = symb_name_next(sy)[id])
        if (symb_spells(symb_text(sy, id), text, len))
            return id;

    const uint16_t id = sy->nnames;

    if (!tabl_room(&sy->names, id + 1u) || !tabl_room(&sy->text, 2u * len + sy->text_free + 1))
        fault_error(sy->fault, E_OM);

    Name *const n = symb_name(sy, id);
    *n = (Name){.text = sy->text_free};

    uint8_t *const d = at(sy->text.at[0] + sy->text_free);
    d[0] = len;

    for (uint8_t i = 0; i < len; i++) {
        d[1 + i] = upcase(text[i]);
        d[1 + len + i] = text[i]; /* as first written: LIST's */
    }

    sy->text_free += 2u * len + 1u;
    symb_name_next(sy)[id] = sy->nhead[h];
    sy->nhead[h] = id;

    if (len > 2 && d[1] == 'F' && d[2] == 'N')
        n->flags = NM_FN;

    sy->nnames++;
    return id;
}

uint16_t symb_find_var(
    Symb *const sy,
    const uint16_t name,
    const uint8_t type,
    const uint8_t kind,
    const uint8_t scope,
    const bool stat
)
{
    const uint32_t h = (name * 5u + type * 2u + (kind >> 7) + scope * 8u) & (HSIZE - 1);

    for (uint16_t id = sy->vhead[h]; id; id = symb_varx(sy, id)->next) {
        const Var *const v = symb_var(sy, id);

        if (v->name == name && v->type == type && (v->info & (VI_ARR | SK_DBL | VK_MASK)) == kind &&
            symb_varx(sy, id)->scope == scope)
            return id;
    }

    const uint16_t id = sy->nvars;

    if (id >= sy->vars.cap && !tabl_room(&sy->vars, id + 1u))
        fault_error(sy->fault, E_OM);

    Var *const v = symb_var(sy, id);
    VarX *const x = symb_varx(sy, id);
    *v = (Var){.name = name, .type = type, .info = kind};

    *x = (VarX){.next = sy->vhead[h], .scope = scope};

    sy->vhead[h] = id;

    *symb_loc(sy, id) = (Loc){};

    *symb_farr(sy, id) = (FrameArr){};

    if (scope && !stat) { /* a local: on its procedure's list, a cell of its frame */
        x->nloc = sy->first_local[scope];
        sy->first_local[scope] = id;
        *symb_loc(sy, id) = loc_local();
    }

    v->vt = symb_cell(sy, id);
    memset(at(v->vt), 0, sizeof(Cell));

    sy->nvars++;
    return id;
}
