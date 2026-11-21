/* The uses of the name at the cursor, lit where they show.
 *
 * # How it works
 * A name is where the highlighter colours one (a variable's, a parameter's, a procedure's being
 * declared): not in a string, a comment or a keyword. The cursor's name is the one it is in or
 * just after. A use is the same name, case aside, with the same type suffix (% & ! # $), or
 * one of the two with none.
 *
 * The name is kept by the cursor's place and its line's text; each line drawn is read for it
 * by its slots, the visible lines' kept ones or worked out (synt_cols).
 */
#include "ui/uses.h"
#include "ui/highlight.h"
#include "ui/screen.h"
#include "util/hash.h"

enum { NAME = 41 }; /* a name's characters at most, its suffix with them */

static struct {
    uint32_t key;
    char name[NAME]; /* empty: none */
    uint8_t len, suffix;
    uint8_t cols[SYNT_KEEP];
} us;

static bool suffix(const uint8_t c) { return c && memchr("%&!#$", c, 5) != nullptr; }

static bool name_slot(const uint8_t s) { return s == SL_NAM || s == SL_PAR || s == SL_FNM; }

/* The name in text, len characters, at i: its base's length to *n, its suffix (0 none) to *sfx;
   false: no name starts at i. */
static bool name_at(
    const uint8_t *const text,
    const uint32_t len,
    const uint8_t *const cols,
    const uint32_t i,
    uint32_t *const n,
    uint8_t *const sfx
)
{
    if (i >= len || i >= SYNT_KEEP || !name_slot(cols[i]) || !is_letter(text[i]))
        return false;
    if (i && is_name_char(text[i - 1]))
        return false;

    uint32_t j = i;

    while (j < len && is_name_char(text[j]))
        j++;

    *n = j - i;
    *sfx = j < len && suffix(text[j]) ? text[j] : 0;
    return true;
}

void uses_find(Session *const S)
{
    const LineBuf *const b = &uimn.edline;
    const uint32_t key = fnv(FNV_SEED ^ (uint32_t)b->col, b->text, b->len);

    if (key == us.key)
        return;

    us.key = key;
    us.len = 0;
    const uint32_t len = b->len < SYNT_KEEP ? b->len : SYNT_KEEP;
    synt_cols(S, b->text, len, us.cols);

    for (uint32_t i = (b->col < len ? b->col : len) + 1; i-- > 0;) { /* the name nearest before */
        uint32_t n;
        uint8_t sfx;

        if (!name_at(b->text, len, us.cols, i, &n, &sfx))
            continue;
        if (b->col > i + n + (sfx != 0)) /* the cursor past it */
            return;
        if (n >= NAME)
            return;

        for (uint32_t k = 0; k < n; k++)
            us.name[k] = (char)upcase(b->text[i + k]);

        us.len = (uint8_t)n;
        us.suffix = sfx;
        return;
    }
}

void uses_paint(
    Session *const S,
    const uint8_t row,
    const uint8_t col,
    const uint8_t *const text,
    const uint32_t len,
    const uint8_t *cols
)
{
    if (!us.len)
        return;

    static uint8_t kept[SYNT_KEEP];

    if (!cols) {
        synt_cols(S, text, len < SYNT_KEEP ? len : SYNT_KEEP, kept);
        cols = kept;
    }

    const uint32_t left = uimn.ed.left, right = left + uimn.ed.width;

    for (uint32_t i = 0; i < len && i < right; i++) {
        uint32_t n;
        uint8_t sfx;

        if (!name_at(text, len, cols, i, &n, &sfx) || n != us.len)
            continue;
        if (sfx && us.suffix && sfx != us.suffix)
            continue;

        bool same = true;

        for (uint32_t k = 0; k < n && same; k++)
            same = upcase(text[i + k]) == (uint8_t)us.name[k];

        const uint32_t end = i + n + (sfx != 0);
        const uint32_t a = i > left ? i : left, b = end < right ? end : right;

        if (same && a < b)
            scrn_attr(row, (uint8_t)(col + a - left), (uint16_t)(b - a), SL_OCC);
    }
}
