/* A stop's values beside the code: each line of the stopped frame's view that shows gets the
 * values of the variables it names, after its end.
 *
 * # How it works
 * The names are where the highlighter colours one (synt_name); a value is the name's Locals row
 * ("total = 47.5"), so it is the selected frame's, named as declared. A record or an array shows
 * nothing, nor a name already shown on the line. The rows are cut at the window's edge.
 */
#include "ui/debuginline.h"
#include "ui/debuglocals.h"
#include "ui/debugpane.h"
#include "ui/highlight.h"
#include "ui/screen.h"

enum { GAP = 2, NSHOWN = 16 }; /* the spaces before each value; the values a line shows at most */

/* s from text column at on screen row row, the window's columns from col; past it and a gap. */
static uint32_t put(const uint8_t row, const uint8_t col, const uint32_t at, const char *const s)
{
    const uint32_t n = (uint32_t)strlen(s), left = uimn.ed.left, right = left + uimn.ed.width;
    const uint32_t skip = at < left ? left - at : 0, from = at + skip;

    if (skip < n && from < right) {
        const uint32_t room = right - from, m = n - skip < room ? n - skip : room;
        scrn_strn(
            row,
            (uint8_t)(col + from - left),
            (const uint8_t *)s + skip,
            (uint16_t)m,
            SL_HINT
        );
    }

    return at + n + GAP;
}

/* Locals' row r is a number's or a string's. */
static bool scalar(const uint8_t r)
{
    const InspValue *const v = dloc_value(r);
    return v && insp_kind(v) == IV_SCALAR;
}

void dinl_paint(
    Session *const S,
    const uint8_t row,
    const uint8_t col,
    const uint8_t *const text,
    const uint32_t len,
    const uint8_t *cols
)
{
    if (!dpan_frame_shown())
        return;

    const uint32_t n = len < SYNT_KEEP ? len : SYNT_KEEP, right = uimn.ed.left + uimn.ed.width;
    cols = synt_need(S, text, n, cols);
    uint8_t shown[NSHOWN];
    uint32_t k = 0, at = len + GAP;

    for (uint32_t i = 0; i < n && at < right && k < NSHOWN; i++) {
        uint32_t w;
        uint8_t sfx;

        if (!synt_name(text, n, cols, i, &w, &sfx))
            continue;

        const uint8_t r = dloc_find(S, text + i, w, sfx);
        i += w;

        if (r == DLOC_NONE || !scalar(r) || memchr(shown, r, k))
            continue;

        shown[k++] = r;
        at = put(row, col, at, dloc_row(r));
    }
}
