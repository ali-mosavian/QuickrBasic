/* The debugger's Locals: the selected frame's procedure's variables, the module's at the module.
 *
 * # How it works
 * The variables and their values are the run's (vm/inspect.h). A row is a variable's name as
 * declared (spelling.c), " = ", its value as the frame sees it. The rows run the parameters first,
 * then by id, the same at each stop; a procedure's, then under a row of its own the module's
 * variables it sees, SHARED.
 *
 * A record or an array is one row, closed (►) or open (▼): an open record's fields follow it, an
 * open array's elements, PAGE at a time, then a row for the rest. A row's path is its expression
 * (p.pts(2).x); its key, the path's hash in its frame's view, names it from stop to stop: which
 * rows are open, and each value's text at the last stop. A value unlike that changed at this stop,
 * drawn in SL_CHG.
 */
#include "ui/debuglocals.h"
#include "vm/inspect.h"
#include "ui/debugtext.h"
#include "ui/spelling.h"
#include "util/hash.h"

enum {
    NROW = 160,  /* the rows at most */
    NOPEN = 64,  /* records and arrays open at most */
    NSEEN = 512, /* values whose last change is kept: a power of two */
    PAGE = 10,   /* an array's elements shown at a time */
    DEPTH = 8,   /* rows nested at most */
    PATH = 128,  /* a row's path or label, 0-ended */
};

enum : uint8_t { GLYPH_OPEN = 0x1F, GLYPH_SHUT = 0x10, INDENT = 3 };

/* A row: a value (a variable's, a field's, an element's), the rest of an open array's, the line
   over the shared variables. */
typedef enum : uint8_t { LR_VALUE, LR_REST, LR_SHARED } RowKind;
typedef struct {
    RowKind kind;
    uint8_t depth;
    uint8_t hi; /* a value changed at this stop: its first column + 1; else 0 */
    bool top;   /* a variable's own row: var is its */
    uint16_t width; /* its text's, uncut */
    InspVar var;
    InspValue value;
    uint32_t key; /* LR_REST: its array's */
    char path[PATH];
    char text[DP_TWMAX];
} LocRow;

/* An open record or array: its elements shown. */
typedef struct {
    uint32_t key;
    uint32_t shown;
} Open;

/* A value's text's hash, the stop it last changed at. */
typedef struct {
    uint32_t key;
    uint32_t hash;
    uint32_t stop;
} Seen;

static struct {
    uint8_t frame;
    uint32_t view; /* the frame's: the keys' scope */
    bool newest;   /* the frame is its procedure's newest, whose values a change is seen in */
    uint8_t n;
    LocRow rows[NROW];
    uint8_t nopen;
    Open open[NOPEN];
    char row[DTXT_ROW]; /* a row being made */
    uint32_t stops;     /* the stops so far */
    uint32_t nseen;     /* seen so far */
    Seen seen[NSEEN];
} loc;

void dloc_stop(void) { loc.stops++; }

uint8_t dloc_count(void) { return loc.n; }

const char *dloc_row(const uint8_t i) { return loc.rows[i].text; }

uint8_t dloc_changed(const uint8_t i) { return loc.rows[i].hi; }

const InspValue *dloc_value(const uint8_t i)
{
    return i < loc.n && loc.rows[i].kind == LR_VALUE ? &loc.rows[i].value : nullptr;
}

const char *dloc_path(const uint8_t i) { return loc.rows[i].path; }

uint16_t dloc_width(const uint8_t i) { return loc.rows[i].width; }

/* Text s's hash from h, letters' case aside when nocase. */
static uint32_t hash(uint32_t h, const char *s, const bool nocase)
{
    for (; *s; s++)
        h = fnv_byte(h, nocase ? upcase((uint8_t)*s) : (uint8_t)*s);

    return h;
}

/* Path path's key: a name's case aside, in the frame's view. */
static uint32_t key_of(const char *const path) { return hash(FNV_SEED ^ loc.view, path, true); }

/* key's entry in the open ones; nullptr shut. */
static Open *opened(const uint32_t key)
{
    for (uint8_t i = 0; i < loc.nopen; i++)
        if (loc.open[i].key == key)
            return &loc.open[i];

    return nullptr;
}

/* Value text s of key is unlike at the stop before: newest frames' only, a first one not. */
static bool changed(const uint32_t key, const char *const s)
{
    if (!loc.newest)
        return false;

    const uint32_t h = hash(FNV_SEED, s, false);

    for (uint32_t i = 0; i < loc.nseen && i < NSEEN; i++) {
        Seen *const e = &loc.seen[i];

        if (e->key != key)
            continue;
        if (e->hash != h)
            *e = (Seen){.key = key, .hash = h, .stop = loc.stops};

        return e->stop == loc.stops;
    }

    loc.seen[loc.nseen++ % NSEEN] = (Seen){.key = key, .hash = h}; /* full: the oldest's place */
    return false;
}

/* s, then t, at d, PATH - 1 characters at most, 0-ended. */
static void join(char d[static PATH], const char *const s, const char *const t)
{
    uint32_t n = 0;

    for (const char *p = s; *p && n < PATH - 1; p++)
        d[n++] = *p;
    for (const char *p = t; *p && n < PATH - 1; p++)
        d[n++] = *p;

    d[n] = 0;
}

/* Array v's subscripts of its element i at d, "(1, 2)"; past it. */
static char *subscripts(const Session *const S, char *d, const InspValue *const v, uint32_t i)
{
    *d++ = '(';

    for (uint8_t k = 0; k < insp_dims(S, v); k++) {
        const uint32_t count = insp_count(S, v, k);

        if (k)
            d = dtxt_cpy(d, ", ");

        d = dtxt_dec(d, insp_lbound(S, v, k) + (int32_t)(i % count));
        i /= count;
    }

    *d++ = ')';
    return d;
}

/* Array v's bounds at d, "(0 to 5, 1 to 3)"; past it. */
static char *bounds(const Session *const S, char *d, const InspValue *const v)
{
    *d++ = '(';

    for (uint8_t k = 0; k < insp_dims(S, v); k++) {
        const int32_t lb = insp_lbound(S, v, k);

        if (k)
            d = dtxt_cpy(d, ", ");

        d = dtxt_dec(dtxt_cpy(dtxt_dec(d, lb), " to "), lb + (int32_t)insp_count(S, v, k) - 1);
    }

    *d++ = ')';
    return d;
}

/* Value v opens: a record, an array made. */
static bool openable(const Session *const S, const InspValue *const v)
{
    return insp_kind(v) == IV_RECORD || (insp_kind(v) == IV_ARRAY && insp_elems(S, v));
}

/* A new row of kind at depth, nullptr no room; loc.row its indent, past it. */
static LocRow *row_new(const RowKind kind, const uint8_t depth, char **const d)
{
    if (loc.n >= NROW)
        return nullptr;

    LocRow *const r = &loc.rows[loc.n++];
    *r = (LocRow){.kind = kind, .depth = depth};

    memset(loc.row, ' ', depth * INDENT);
    *d = loc.row + depth * INDENT;
    return r;
}

static void value_rows(
    Session *S,
    const InspValue *v,
    const char *label,
    const char *path,
    uint8_t depth
);

/* Record v's fields' rows, under its row at path. */
static void fields(
    Session *const S,
    const InspValue *const v,
    const char *const path,
    const uint8_t depth
)
{
    for (uint32_t i = 0; i < insp_nfields(S, v); i++) {
        uint32_t name;
        const InspValue f = insp_field(S, v, i, &name);
        char label[PATH], sub[PATH];
        *spel_name(S, dtxt_cpy(label, "."), name, SPEL_FIELD) = 0;
        join(sub, path, label);
        value_rows(S, &f, label, sub, depth + 1);
    }
}

/* Array v's elements' rows, its shown ones, under its row at path of key; then the rest's. */
static void elements(
    Session *const S,
    const InspValue *const v,
    const char *const path,
    const uint32_t key,
    const uint8_t depth
)
{
    const uint32_t n = insp_elems(S, v);
    const uint32_t shown = opened(key)->shown < n ? opened(key)->shown : n;

    for (uint32_t i = 0; i < shown; i++) {
        const InspValue e = insp_elem(S, v, i);
        char label[PATH], sub[PATH];
        *subscripts(S, label, v, i) = 0;
        join(sub, path, label);
        value_rows(S, &e, label, sub, depth + 1);
    }

    char *d;
    LocRow *const r = shown < n ? row_new(LR_REST, depth + 1, &d) : nullptr;

    if (!r)
        return;

    d = subscripts(S, dtxt_cpy(d, "... "), v, shown);
    *subscripts(S, dtxt_cpy(d, " to "), v, n - 1) = 0;
    r->value = *v;
    r->key = key;
    dtxt_cut(r->text, loc.row);
}

/* Value v's row, label its name ("p", ".x", "(3)"), path its expression; an open one's under it. */
static void value_rows(
    Session *const S,
    const InspValue *const v,
    const char *const label,
    const char *const path,
    const uint8_t depth
)
{
    char *d;
    LocRow *const r = row_new(LR_VALUE, depth, &d);

    if (!r)
        return;

    const InspKind kind = insp_kind(v);
    const bool can = openable(S, v) && depth < DEPTH;
    r->value = *v;
    r->key = key_of(path);
    join(r->path, path, "");
    const bool open = can && opened(r->key);

    if (can)
        *d++ = (char)(open ? GLYPH_OPEN : GLYPH_SHUT);

    d = dtxt_cpy(d, label);

    if (kind == IV_RECORD) {
        d = spel_name(S, dtxt_cpy(d, " as "), insp_type(S, v), SPEL_MODULE);
    } else if (kind == IV_ARRAY) {
        d = bounds(S, d, v);
    } else if (kind == IV_SCALAR) {
        d = dtxt_cpy(d, " = ");
        const uint8_t col = (uint8_t)(d - loc.row);
        *(d = insp_text(S, v, d, DP_TWMAX)) = 0;
        r->hi = changed(r->key, loc.row + col) && col < DP_TWMAX - 1 ? col + 1 : 0;
    }

    *d = 0;
    r->width = (uint16_t)(d - loc.row);
    dtxt_cut(r->text, loc.row);

    if (open && kind == IV_RECORD)
        fields(S, v, path, depth);
    else if (open)
        elements(S, v, path, r->key, depth);
}

/* Variable var's rows. */
static void var_rows(Session *const S, const InspVar *const var)
{
    char name[PATH];
    *spel_var(S, name, var) = 0;
    const uint8_t at = loc.n;
    const InspValue v = insp_var(S, loc.frame, var);
    value_rows(S, &v, name, name, 0);

    if (at < loc.n) {
        loc.rows[at].top = true;
        loc.rows[at].var = *var;
    }
}

void dloc_read(Session *const S, const uint8_t f)
{
    const uint32_t view = dstk_at(f)->view;
    loc.frame = f;
    loc.view = view == DSTK_NOVIEW ? 0 : view;
    loc.newest = insp_newest(S, f);
    loc.n = 0;
    spel_read(S, loc.view);
    InspVar vars[NROW];
    const uint32_t n = insp_vars(S, f, vars, NROW);

    for (uint32_t i = 0; i < n; i++)
        var_rows(S, &vars[i]);

    const uint32_t ns = insp_shared(S, f, vars, NROW);
    char *d;
    LocRow *const r = ns ? row_new(LR_SHARED, 0, &d) : nullptr;

    if (r)
        dtxt_cut(r->text, "\xC4\xC4\xC4 shared \xC4\xC4\xC4");

    for (uint32_t i = 0; i < ns; i++)
        var_rows(S, &vars[i]);
}

bool dloc_open(Session *const S, const uint8_t i)
{
    if (i >= loc.n)
        return false;

    const LocRow *const r = &loc.rows[i];
    Open *const o = opened(r->key);

    if (r->kind == LR_REST)
        o->shown += PAGE;
    else if (o || !openable(S, &r->value) || r->depth >= DEPTH)
        return false;
    else
        loc.open[loc.nopen < NOPEN ? loc.nopen++ : 0] = (Open){.key = r->key, .shown = PAGE};

    dloc_read(S, loc.frame);
    return true;
}

uint8_t dloc_close(Session *const S, uint8_t i)
{
    if (i >= loc.n)
        return i;

    if (loc.rows[i].kind != LR_VALUE || !opened(loc.rows[i].key)) { /* its parent, open */
        const uint8_t depth = loc.rows[i].depth;

        if (!depth)
            return i;

        while (loc.rows[i].depth >= depth)
            i--;
    }

    *opened(loc.rows[i].key) = loc.open[--loc.nopen];
    dloc_read(S, loc.frame);
    return i;
}

const char *dloc_find(Session *const S, const uint8_t *const s, const uint32_t n, const uint8_t sfx)
{
    for (uint8_t i = 0; i < loc.n; i++)
        if (loc.rows[i].top && insp_is(S, &loc.rows[i].var, s, n, sfx))
            return loc.rows[i].text;

    return nullptr;
}
