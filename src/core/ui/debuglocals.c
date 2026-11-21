/* The debugger's Locals: the selected frame's procedure's variables, the module's at the module.
 *
 * # How it works
 * The variables and their values are the run's (vm/inspect.h). A row is a variable's name as
 * declared (spelling.c), " = ", its value as the frame sees it. The rows run the parameters first,
 * then by id, the same at each stop. A change is a row's text unlike the one its variable had at the
 * stop before, kept in seen by id and name (a relinked program's id may be another's); its value is
 * drawn in SL_CHG.
 */
#include "ui/debuglocals.h"
#include "vm/inspect.h"
#include "ui/debugtext.h"
#include "ui/spelling.h"

enum {
    NDVAR = 64,  /* the variables shown at most */
    NSEEN = 256, /* variables whose last change is kept */
};

/* A variable's row at the stop it last changed at: its text's hash, that stop. */
typedef struct {
    uint32_t id, name;
    uint32_t hash;
    uint32_t stop;
} Seen;

static struct {
    uint8_t frame;
    uint8_t n;
    InspVar var[NDVAR]; /* the rows' variables */
    uint8_t vhi[NDVAR]; /* a value changed at this stop: its first column + 1; else 0 */
    char vt[NDVAR][DP_TWMAX];
    char row[DTXT_ROW]; /* a row being made */
    char tip[DP_TWMAX]; /* a module variable's row, dloc_find's */
    uint32_t stops;     /* the stops so far */
    uint16_t nseen;
    Seen seen[NSEEN];
} loc;

void dloc_stop(void) { loc.stops++; }

uint8_t dloc_count(void) { return loc.n; }

const char *dloc_row(const uint8_t i) { return loc.vt[i]; }

uint8_t dloc_changed(const uint8_t i) { return loc.vhi[i]; }

/* Variable var's row in loc.row: its name, " = ", its value as the frame sees it. Its value's
   first column. */
static uint8_t var_text(Session *const S, const InspVar *const var)
{
    char *d = spel_name(S, loc.row, var->name, var->local ? SPEL_LOCAL : SPEL_MODULE);

    if (var->sfx)
        *d++ = var->sfx;

    d = dtxt_cpy(d, " = ");
    const uint8_t col = (uint8_t)(d - loc.row);
    const InspValue v = insp_var(S, loc.frame, var);

    if (v.kind == IV_ARRAY)
        d = dtxt_cpy(d, "(array)");
    else if (v.kind != IV_SCALAR)
        d = dtxt_cpy(d, "(record)");
    else
        d = insp_text(S, &v, d, DP_TWMAX);

    *d = 0;
    return col;
}

static uint32_t hash(const char *s)
{
    uint32_t h = 2166136261u;

    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;

    return h;
}

/* The stop variable var's row last changed at, its text in loc.row; newest: its frame is its
   procedure's newest, whose values a change is seen in. 0: not changed since first seen. */
static uint32_t changed_at(const InspVar *const var, const bool newest)
{
    const uint32_t h = hash(loc.row);
    Seen *e = nullptr;

    for (uint16_t i = 0; i < loc.nseen && !e; i++)
        if (loc.seen[i].id == var->id)
            e = &loc.seen[i];

    if (e && e->name != var->name) { /* another program's */
        *e = (Seen){.id = var->id, .name = var->name, .hash = h};

        return 0;
    }

    if (!newest)
        return e ? e->stop : 0;
    if (!e) {
        if (loc.nseen >= NSEEN)
            return 0;

        e = &loc.seen[loc.nseen++];
        *e = (Seen){.id = var->id, .name = var->name, .hash = h};
    } else if (e->hash != h) {
        e->hash = h;
        e->stop = loc.stops;
    }

    return e->stop;
}

void dloc_read(Session *const S, const uint8_t f)
{
    const uint32_t view = dstk_at(f)->view;
    const bool newest = insp_newest(S, f);
    loc.frame = f;
    spel_read(S, view == DSTK_NOVIEW ? 0 : view);
    loc.n = (uint8_t)insp_vars(S, f, loc.var, NDVAR);

    for (uint8_t i = 0; i < loc.n; i++) {
        const uint8_t col = var_text(S, &loc.var[i]);
        const bool changed = changed_at(&loc.var[i], newest) == loc.stops && newest;
        loc.vhi[i] = changed && col < DP_TWMAX - 1 ? col + 1 : 0;
        dtxt_cut(loc.vt[i], loc.row);
    }
}

const char *dloc_find(Session *const S, const uint8_t *const s, const uint32_t n, const uint8_t sfx)
{
    for (uint8_t i = 0; i < loc.n; i++)
        if (insp_is(S, &loc.var[i], s, n, sfx))
            return loc.vt[i];

    InspVar shared[NDVAR];
    const uint32_t ns = insp_shared(S, loc.frame, shared, NDVAR);

    for (uint32_t i = 0; i < ns; i++) {
        if (!insp_is(S, &shared[i], s, n, sfx))
            continue;

        var_text(S, &shared[i]);
        dtxt_cut(loc.tip, loc.row);
        return loc.tip;
    }

    return nullptr;
}
