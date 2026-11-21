/* The debugger's Locals: the selected frame's procedure's variables, the module's at the module.
 *
 * # How it works
 * A row is a variable's name, " = ", its value as the frame sees it (session_cell). The rows run the
 * last changed first, else the parameters first, then by id. A change is a row's text unlike the
 * one its variable had at the stop before, kept in seen by id and name (a relinked program's id may
 * be another's).
 */
#include "ui/debuglocals.h"
#include "comp/blocks.h"
#include "mem/scope.h"
#include "mem/symbols.h"
#include "ui/debugvalue.h"

enum {
    NDVAR = 64,  /* the variables shown at most */
    VK_FN = 3,   /* a DEF FN's kind (Var.info) */
    NSEEN = 256, /* variables whose last change is kept */
};

/* A variable's row at the stop it last changed at: its text's hash, that stop. */
typedef struct {
    uint32_t id, name;
    uint32_t hash;
    uint32_t stop;
} Seen;

static const char sfx_char[] = "%&!#$";                                /* a suffix, */
static const uint8_t sfx_type[] = {ET_I2, ET_I4, ET_R4, ET_R8, ET_SD}; /* its type */
static const char sfx_of[] = {0, '%', '&', 0, '#', '$'};               /* by ET_*: the one shown */

static struct {
    DebugFrame frame;
    uint32_t proc; /* the frame's procedure, 0 the module */
    uint8_t n;
    uint32_t vid[NDVAR];   /* the rows' variables */
    uint32_t vstop[NDVAR]; /* their last changes */
    char vt[NDVAR][DP_TWMAX];
    char row[DVAL_ROW]; /* a row being made */
    char tip[DP_TWMAX]; /* a module variable's row, dloc_find's */
    uint32_t stops;     /* the stops so far */
    uint16_t nseen;
    Seen seen[NSEEN];
} loc;

void dloc_stop(void) { loc.stops++; }

uint8_t dloc_count(void) { return loc.n; }

const char *dloc_row(const uint8_t i) { return loc.vt[i]; }

/* id is a parameter of procedure loc.proc. */
static bool is_param(Session *const S, const uint32_t id)
{
    if (!loc.proc)
        return false;

    const Proc *const p = blok_proc(&S->blok, loc.proc);

    for (uint16_t k = 0; k < p->npar; k++)
        if (p->params[k].var == id)
            return true;

    return false;
}

/* Variable id's row in loc.row: its name, " = ", its value as the frame sees it; par its
   parameter's place or -1. */
static void var_text(Session *const S, const uint32_t id, const int32_t par)
{
    const Var *const v = symb_var(&S->symb, id);
    const uint8_t *const name = symb_text(&S->symb, v->name);
    char *d = loc.row;
    memcpy(d, name + 1, name[0]);
    d += name[0];

    if (v->type <= ET_SD && sfx_of[v->type])
        *d++ = sfx_of[v->type];

    d = dval_cpy(d, " = ");

    if (v->info & VI_ARR)
        d = dval_cpy(d, "(array)");
    else if (v->type >= ET_REC)
        d = dval_cpy(d, "(record)");
    else
        d = dval_var(S, d, id, session_cell(S, &loc.frame, id, par));

    *d = 0;
}

static uint32_t hash(const char *s)
{
    uint32_t h = 2166136261u;

    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;

    return h;
}

/* The stop variable id's row last changed at, its text in loc.row; newest as dloc_read's. 0: not
   changed since first seen. */
static uint32_t changed_at(Session *const S, const uint32_t id, const bool newest)
{
    const uint32_t h = hash(loc.row);
    const uint32_t name = symb_var(&S->symb, id)->name;
    Seen *e = nullptr;

    for (uint16_t i = 0; i < loc.nseen && !e; i++)
        if (loc.seen[i].id == id)
            e = &loc.seen[i];

    if (e && e->name != name) { /* another program's */
        *e = (Seen){.id = id, .name = name, .hash = h};

        return 0;
    }

    if (!newest)
        return e ? e->stop : 0;
    if (!e) {
        if (loc.nseen >= NSEEN)
            return 0;

        e = &loc.seen[loc.nseen++];
        *e = (Seen){.id = id, .name = name, .hash = h};
    } else if (e->hash != h) {
        e->hash = h;
        e->stop = loc.stops;
    }

    return e->stop;
}

/* The next row, after the rows changed as late or later: variable id, parameter par of its
   procedure or -1. */
static void var1(Session *const S, const uint32_t id, const int32_t par, const bool newest)
{
    if (loc.n >= NDVAR)
        return;

    var_text(S, id, par);
    const uint32_t stop = changed_at(S, id, newest);
    uint8_t i = loc.n++;

    for (; i && loc.vstop[i - 1] < stop; i--) {
        loc.vid[i] = loc.vid[i - 1];
        loc.vstop[i] = loc.vstop[i - 1];
        memcpy(loc.vt[i], loc.vt[i - 1], sizeof loc.vt[i]);
    }

    loc.vid[i] = id;
    loc.vstop[i] = stop;
    dval_cut(loc.vt[i], loc.row);
}

/* A variable Locals and the tooltip leave out: a DEF FN's, a FOR loop's hidden limit or step. */
static bool skipped(Session *const S, const uint32_t id)
{
    const Var *const v = symb_var(&S->symb, id);
    return (v->info & (VI_ARR | VK_MASK)) == VK_FN || symb_text(&S->symb, v->name)[1] == LOOP_NAME;
}

void dloc_read(Session *const S, const DebugFrame *const f, const bool newest)
{
    loc.frame = *f;
    loc.proc = f->proc;
    loc.n = 0;

    if (loc.proc) {
        const Proc *const p = blok_proc(&S->blok, loc.proc);

        for (uint16_t k = 0; k < p->npar; k++)
            var1(S, p->params[k].var, k, newest);
    }

    for (uint32_t id = 1; id < S->symb.nvars; id++)
        if (symb_varx(&S->symb, id)->scope == loc.proc && !skipped(S, id) && !is_param(S, id))
            var1(S, id, -1, newest);
}

/* Variable id is the name s, n long, suffix sfx: its letters the name's, the suffix, if any, its
   type's. */
static bool match(
    Session *const S,
    const uint32_t id,
    const uint8_t *const s,
    const uint32_t n,
    const uint8_t sfx
)
{
    const Var *const v = symb_var(&S->symb, id);

    if (sfx && sfx_type[strchr(sfx_char, sfx) - sfx_char] != v->type)
        return false;

    const uint8_t *const t = symb_text(&S->symb, v->name);

    if ((uint8_t)n != t[0])
        return false;
    for (uint32_t i = 0; i < n; i++)
        if (upcase(s[i]) != t[1 + i])
            return false;

    return true;
}

/* Module variable id is seen in procedure loc.proc: DIM SHARED, or SHARED there. */
static bool seen(Session *const S, const uint32_t id)
{
    const uint32_t name = symb_var(&S->symb, id)->name;
    const Binding *b = scop_get(&S->scop, 0, name);

    if (b && b->flags & BF_SHR)
        return true;

    b = scop_get(&S->scop, blok_proc(&S->blok, loc.proc)->name, name);
    return b && b->flags & BF_REF;
}

const char *dloc_find(Session *const S, const uint8_t *const s, const uint32_t n, const uint8_t sfx)
{
    for (uint8_t i = 0; i < loc.n; i++)
        if (match(S, loc.vid[i], s, n, sfx))
            return loc.vt[i];
    if (!loc.proc) /* the module's: all in Locals */
        return nullptr;
    for (uint32_t id = 1; id < S->symb.nvars; id++) {
        if (symb_varx(&S->symb, id)->scope || skipped(S, id) || !match(S, id, s, n, sfx) ||
            !seen(S, id))
            continue;

        var_text(S, id, -1);
        dval_cut(loc.tip, loc.row);
        return loc.tip;
    }

    return nullptr;
}
