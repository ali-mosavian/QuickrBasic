/* The debugger's call stack: the stop's frames, each with its line and the row it shows.
 *
 * # How it works
 * The frames are the run's (vm/inspect.h), NDFRM at most, then a row for how many more. A frame's
 * row is its view's name, its arguments as they are now, then "line n" from column FRAME_LINE at
 * least, n the line's in its view.
 */
#include "ui/debugstack.h"
#include "ui/debugtext.h"
#include "ui/linemap.h"
#include "ui/spelling.h"
#include "ui/text.h"
#include "ui/view.h"

enum {
    NDFRM = 16,      /* the frames shown at most */
    NPAR = 16,       /* a frame's arguments shown at most */
    FRAME_LINE = 16, /* a frame's row: "line n" from column 16 at least */
    ARG_TEXT = 10,   /* an argument's string: its first 10 characters */
};

static struct {
    uint8_t n;
    StackFrame fr[NDFRM];
    char more[DP_TWMAX]; /* the row past them: how many more; "" none */
} stk;

/* Frame i's view, DSTK_NOVIEW the immediate window's; its text line to *tl. */
static uint32_t frame_view(Session *const S, const uint32_t i, uint32_t *const tl)
{
    const uint32_t pline = insp_line(S, i);
    return pline != DLINE && lmap_of(S, pline, tl) ? view_of(*tl) : DSTK_NOVIEW;
}

static const char *view_title(const uint32_t v)
{
    return v == DSTK_NOVIEW ? "Immediate" : v ? view_name(v) : "module";
}

/* Frame i's arguments at d, " (n% = 2, a())", none nothing, from row's start DP_TWMAX characters
   at most and those past; past it. */
static char *args(Session *const S, const char *const row, char *d, const uint32_t i)
{
    InspVar par[NPAR];
    const uint32_t n = insp_params(S, i, par, NPAR);

    for (uint32_t k = 0; k < n && d - row < DP_TWMAX; k++) {
        const InspValue v = insp_var(S, i, &par[k]);
        d = spel_var(S, dtxt_cpy(d, k ? ", " : " ("), &par[k]);

        if (insp_kind(&v) == IV_SCALAR)
            d = insp_text(S, &v, dtxt_cpy(d, " = "), ARG_TEXT);
        else if (insp_kind(&v) == IV_ARRAY)
            d = dtxt_cpy(d, "()");
    }

    return n ? dtxt_cpy(d, ")") : d;
}

/* Frame f of the run's frame i: its text line, view and row. */
static void frame1(Session *const S, StackFrame *const f, const uint32_t i)
{
    uint32_t tl;
    const uint32_t v = frame_view(S, i, &tl);
    char row[DTXT_ROW];
    f->view = v;
    f->line = -1;

    if (v == DSTK_NOVIEW) {
        dtxt_cut(f->text, view_title(v));
        return;
    }

    f->line = (int32_t)tl;
    spel_read(S, v);
    const uint32_t proc = insp_scope(S, i);
    char *d = text_dialect_at(S, tl)->text && proc /* a fn's, its view its file's */
                  ? spel_name(S, row, blok_proc(&S->blok, proc)->name, SPEL_MODULE)
                  : dtxt_cpy(row, view_title(v));
    d = args(S, row, d, i);
    const uint32_t at = (uint32_t)(d - row) < FRAME_LINE ? FRAME_LINE : (uint32_t)(d - row) + 1;
    memset(d, ' ', row + at - d);
    *dtxt_dec(dtxt_cpy(row + at, "line "), (int32_t)(tl - view_at(v)->first + 1)) = 0;
    dtxt_cut(f->text, row);
}

void dstk_read(Session *const S)
{
    const uint32_t n = insp_frames(S);
    stk.n = (uint8_t)(n < NDFRM ? n : NDFRM);
    stk.more[0] = 0;

    for (uint8_t i = 0; i < stk.n; i++)
        frame1(S, &stk.fr[i], i);

    if (n <= NDFRM)
        return;

    char row[DTXT_ROW];
    uint32_t tl;
    char *const d = dtxt_cpy(dtxt_dec(dtxt_cpy(row, "\x19 "), (int32_t)(n - NDFRM)), " more, ");
    *dtxt_cpy(dtxt_cpy(d, view_title(frame_view(S, n - 1, &tl))), " last") = 0;
    dtxt_cut(stk.more, row);
}

uint8_t dstk_count(void) { return stk.n; }

const StackFrame *dstk_at(const uint8_t i) { return &stk.fr[i]; }

uint8_t dstk_rows(void) { return stk.n + !!stk.more[0]; }

const char *dstk_row(const uint8_t i) { return i < stk.n ? stk.fr[i].text : stk.more; }
