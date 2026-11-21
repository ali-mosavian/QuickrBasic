/* The debugger's Watch: expressions, each PRINTed at each stop; Break When's halt the run.
 *
 * # How it works
 * A watch is PRINTed in the stopped frame's procedure's scope (vm/inspect.h); its row is what PRINT shows,
 * its spaces and end of line left out. Watches are kept across runs.
 *
 * A Break When's watch is also PRINTed as the run goes, its row marked ♦: one of any line at each
 * statement its scope runs (dwat_halts), one of a line at that line's breakpoint (dwat_breaks). It
 * stops the run when its value changes, or, WK_TRUE, when it turns true (at a line, each time it is
 * true); the value it was last PRINTed with, a stop's too, is the one a change is from. A line's
 * is its breakpoint's: the nth of the text's marked lines, kept to it as marks come and go
 * (dwat_marks).
 */
#include "ui/debugwatch.h"
#include "session.h"
#include "ui/debugtext.h"
#include "ui/text.h"
#include "vm/inspect.h"

enum {
    NWATCH = 8, /* watches at most */
    WXLEN = 41, /* a watch's expression, 0-ended */
};

typedef struct {
    BreakWhen how;
    bool halts; /* a Break When's */
    bool seen;  /* PRINTed once: hash, truth its value's */
    bool truth;
    uint32_t hash;
    char expr[WXLEN];
    char row[DP_TWMAX];
} Watch;

static struct {
    uint8_t n;
    Watch w[NWATCH];
    char row[DTXT_ROW];
} wat;

uint8_t dwat_count(void) { return wat.n; }

const char *dwat_row(const uint8_t i) { return wat.w[i].row; }

/* session_eval's output out at d, its spaces and end of line left out; past it. */
static char *eval_text(char *d, const char *const out)
{
    uint32_t a = 0;

    while (out[a] == ' ')
        a++;

    uint32_t n = 0;

    for (; (uint8_t)out[a + n] >= ' '; n++)
        d[n] = out[a + n];

    d += n;

    while (d[-1] == ' ') /* may eat into " = " for an empty string, as the asm */
        d--;

    return d;
}

static uint32_t hash(const char *s)
{
    uint32_t h = 2166136261u;

    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;

    return h;
}

/* PRINT's text t is of a number not 0. */
static bool is_true(const char *const t)
{
    return (t[0] == '-' || t[0] == '.' || (t[0] >= '0' && t[0] <= '9')) && strcmp(t, "0");
}

/* Watch w's value in scope as text at d: its error's message instead, false. Past it to *end. A
   ':' (another statement): none. */
static bool value(
    Session *const S,
    const Watch *const w,
    const uint32_t scope,
    char *const d,
    char **const end
)
{
    char out[80];

    if (strchr(w->expr, ':')) {
        *end = dtxt_cpy(d, session_errtext(S, E_SN));
        return false;
    }

    const Status st = insp_eval_in(S, scope, w->expr, out);
    *end = st != ST_OK ? dtxt_cpy(d, session_errtext(S, (uint8_t)st)) : eval_text(d, out);
    return st == ST_OK;
}

/* Watch w's value now, text t, kept as the one a change is from: it stops the run, its value
   changed, or turned true (level: is true). */
static bool keep(Watch *const w, const char *const t, const bool level)
{
    const uint32_t h = hash(t);
    const bool truth = is_true(t), was = w->seen && w->truth, changed = w->seen && h != w->hash;
    w->seen = true;
    w->truth = truth;
    w->hash = h;
    return w->how.kind == WK_CHANGE ? changed : truth && (level || !was);
}

/* Watch w PRINTed in scope: it stops the run (keep's); an error does not. */
static bool trips(Session *const S, Watch *const w, const uint32_t scope, const bool level)
{
    char t[DTXT_ROW], *end;

    if (!value(S, w, scope, t, &end))
        return false;

    *end = 0;
    return keep(w, t, level);
}

/* Watch w's scope: a Break When's of any line its own, another's the stopped frame's; false the
   stop not in it. */
static bool scope_of(const Session *const S, const Watch *const w, uint32_t *const scope)
{
    const uint32_t here = insp_scope(S, 0);
    *scope = w->halts && !w->how.line ? w->how.scope : here;
    return !*scope || *scope == here;
}

void dwat_eval(Session *const S)
{
    for (uint8_t i = 0; i < wat.n; i++) {
        Watch *const w = &wat.w[i];
        char *d = dtxt_cpy(dtxt_cpy(dtxt_cpy(wat.row, w->halts ? "\x04" : ""), w->expr), " = ");
        char *const v = d;
        uint32_t scope;

        if (session_in_direct(S) || !scope_of(S, w, &scope)) {
            *(d = dtxt_cpy(d, "(not here)")) = 0;
        } else {
            const bool ok = value(S, w, scope, d, &d);
            *d = 0;

            if (ok && w->halts && !w->how.line)
                keep(w, v, false);
        }

        dtxt_cut(w->row, wat.row);
    }
}

/* Watch expr, n long, of how, a Break When's when halts; false: no room. */
static bool add(const uint8_t *const expr, const uint32_t n, const BreakWhen how, const bool halts)
{
    if (wat.n >= NWATCH)
        return false;

    Watch *const w = &wat.w[wat.n++];
    const uint32_t len = n < WXLEN - 1 ? n : WXLEN - 1;
    *w = (Watch){.how = how, .halts = halts};

    memcpy(w->expr, expr, len);
    w->expr[len] = 0;
    return true;
}

/* The run halting while a Break When of any line is set. */
static void halting(Session *const S)
{
    bool any = false;

    for (uint8_t i = 0; i < wat.n; i++)
        any |= wat.w[i].halts && !wat.w[i].how.line;

    session_halting(S, any);
}

bool dwat_add(const uint8_t *const expr, const uint32_t n)
{
    return add(expr, n, (BreakWhen){}, false);
}

bool dwat_break(Session *const S, const uint8_t *const expr, const uint32_t n, const BreakWhen how)
{
    for (uint8_t i = 0; how.line && i < wat.n; i++) /* its line's, in place of the one it had */
        if (wat.w[i].halts && wat.w[i].how.line && wat.w[i].how.nth == how.nth)
            dwat_del(S, i);

    const bool ok = add(expr, n, how, true);
    halting(S);
    return ok;
}

/* Watch i dropped, the run's halting not told. */
static void drop(const uint8_t i)
{
    wat.n--;
    memmove(&wat.w[i], &wat.w[i + 1], (wat.n - i) * sizeof wat.w[0]);
}

void dwat_del(Session *const S, const uint8_t i)
{
    if (i >= wat.n)
        return;

    drop(i);
    halting(S);
}

bool dwat_mark(const uint8_t i, uint32_t *const nth)
{
    if (i >= wat.n || !wat.w[i].halts || !wat.w[i].how.line)
        return false;

    *nth = wat.w[i].how.nth;
    return true;
}

bool dwat_halts(Session *const S)
{
    if (session_in_direct(S))
        return false;

    bool stop = false;

    for (uint8_t i = 0; i < wat.n; i++) {
        Watch *const w = &wat.w[i];
        uint32_t scope;

        if (w->halts && !w->how.line && scope_of(S, w, &scope))
            stop |= trips(S, w, scope, false);
    }

    return stop;
}

bool dwat_breaks(Session *const S, const uint32_t nth)
{
    for (uint8_t i = 0; i < wat.n; i++) {
        Watch *const w = &wat.w[i];

        if (w->halts && w->how.line && w->how.nth == nth)
            return !session_in_direct(S) && trips(S, w, insp_scope(S, 0), true);
    }

    return true;
}

bool dwat_conditional(const uint32_t nth)
{
    for (uint8_t i = 0; i < wat.n; i++)
        if (wat.w[i].halts && wat.w[i].how.line && wat.w[i].how.nth == nth)
            return true;

    return false;
}

void dwat_marks(const uint32_t nth, const int32_t delta)
{
    for (uint8_t i = 0; i < wat.n;) {
        BreakWhen *const how = &wat.w[i].how;

        if (wat.w[i].halts && how->line && (nth == TEXT_MARKS || (delta < 0 && how->nth == nth))) {
            drop(i);
            continue;
        }

        if (wat.w[i].halts && how->line && how->nth >= nth)
            how->nth += (uint32_t)delta;

        i++;
    }
}
