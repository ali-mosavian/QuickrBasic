/* Enter after a block's opener: its end put in below (NEXT after FOR, end fn after a lambda's
 * header), the cursor on a line between, a step in.
 *
 * # How it works
 * The line's block keywords (indt_blocks, its dialect's) up to the cursor say which blocks it
 * leaves open; each opener carries its end as typed. A block is ended already when, down the
 * lines below, the first end of its kind not paired with an opener of its kind between is at the
 * opener's column: an end at another column closes an outer block. Only at the line's end, or
 * before closing brackets alone, which the last end then takes: fn(x) ) gives end fn).
 *
 * # Example
 *   FOR i = 1 TO 3|      Enter:  FOR i = 1 TO 3
 *                                    |
 *                                NEXT
 */
#include "ui/blockends.h"
#include "ui/ui.h"

/* Is text's rest from col to n blanks and closing brackets alone? */
static bool bend_rest_closes(const uint8_t *const text, const uint16_t n, const uint16_t col)
{
    for (uint16_t i = col; i < n; i++)
        if (text[i] != ' ' && text[i] != ')' && text[i] != ']' && text[i] != '}')
            return false;

    return true;
}

/* Does a line below line end the block of kind kind opened at column at? */
static bool bend_ended(
    const IndtLines *const ls,
    const uint32_t line,
    const uint32_t count,
    const uint8_t kind,
    const uint16_t at
)
{
    uint8_t text[LNBUF];
    uint32_t depth = 0;

    for (uint32_t l = line + 1; l < count; l++) {
        const uint16_t len = ls->get(ls->ctx, l, text);
        DialBlock ev[DIAL_NBLOCK];
        const uint32_t n = indt_blocks(ls, text, len, ev);

        for (uint32_t e = 0; e < n; e++) {
            if (ev[e].kind != kind || ev[e].what == DBK_MID)
                continue;
            if (ev[e].what == DBK_OPEN)
                depth++;
            else if (depth)
                depth--;
            else
                return indt_lead(text, len) == at;
        }
    }

    return false;
}

uint32_t bend_missing(
    const IndtLines *const ls,
    const uint32_t line,
    const uint32_t count,
    const uint8_t *const text,
    const uint16_t n,
    const uint16_t col,
    const char **const ends
)
{
    if (!bend_rest_closes(text, n, col))
        return 0;

    DialBlock ev[DIAL_NBLOCK];
    const uint32_t nev = indt_blocks(ls, text, col, ev);
    const DialBlock *open[DIAL_NBLOCK];
    uint32_t nopen = 0;

    for (uint32_t e = 0; e < nev; e++)
        if (ev[e].what == DBK_OPEN)
            open[nopen++] = &ev[e];
        else if (ev[e].what == DBK_END && nopen && open[nopen - 1]->kind == ev[e].kind)
            nopen--;

    const uint16_t at = indt_lead(text, n);
    uint32_t k = 0;

    while (nopen--)
        if (open[nopen]->end && !bend_ended(ls, line, count, open[nopen]->kind, at))
            ends[k++] = open[nopen]->end;

    return k;
}
