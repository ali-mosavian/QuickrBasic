/* Syntax highlighting (src/ui/syntax.asm). The line's body, each item at its column
   (lexr.col), gives its slot to the columns from there to the next item's. state: what the last
   item makes of the next: after AS or TYPE a type; after SUB, FUNCTION, DEF a name being
   declared, then its parameters in the ( ) after it, depth deep. */
#include "ui/highlight.h"
#include "session.h"
#include "ui/screen.h"
#include "comp/comp.h"
#include "comp/dfa.h"
#include "comp/lexer.h"

Synt synt = {.on = true};

enum : uint8_t { HS_AS = 1, HS_DEF = 2, HS_NAME = 4 };
enum : uint8_t { TK_MIN = 0x80 }; /* a token's first byte from here */
enum { DATA_LEN = 4 };            /* "DATA" */

static struct {
    uint8_t state, depth;
    uint32_t len;
    uint8_t prev_slot; /* the last item's slot */
    uint32_t prev_col; /* its column */
    uint8_t cols[256]; /* the slots of the columns shown */
} hl;

void synt_toggle(void) { synt.on = !synt.on; }

/* slot from column col on; the item before it to col in its own, the columns shown only. */
static void paint(const uint32_t col, const uint8_t slot)
{
    const int32_t left = uimn.ed.left;
    int32_t from = (int32_t)hl.prev_col, to = left + uimn.ed.width;

    if (from < left)
        from = left;
    if ((uint32_t)to > col)
        to = (int32_t)col;
    if (to > from)
        memset(hl.cols + (from - left), hl.prev_slot, (size_t)(to - from));

    hl.prev_col = col;
    hl.prev_slot = slot;
}

/* A number kept as its text: past it. */
static const uint8_t *number(const uint8_t *const p)
{
    for (size_t i = 0;; i++) {
        const uint8_t c = p[i];

        if (!is_digit(c) && c != '.' && c != 'E' && c != 'D' && c != '+' && c != '-' && c != '#' &&
            c != '!')
            return p + i;
    }
}

/* Past DATA's items' ':' out of quotes; nullptr: the body's end. */
static const uint8_t *data_end(const uint8_t *const p)
{
    bool quoted = false;

    for (size_t i = 0; p[i]; i++) {
        if (p[i] == '"')
            quoted = !quoted;
        if (!quoted && p[i] == ':')
            return p + i + 1;
    }

    return nullptr;
}

/* Past a string's closing quote, or at the body's end. */
static const uint8_t *string_end(const uint8_t *const p)
{
    size_t i = 0;

    while (p[i] && p[i++] != '"')
        ;

    return p + i;
}

/* A token's slot (80h .. FEh); *state the next item's. */
static uint8_t token(const uint8_t t, uint8_t *const state)
{
    if ((t >= TK_PLUS && t <= TK_POW) || (t >= TK_GT && t <= TK_LT)) /* + - * / ^ > = < */
        return SL_OP;
    if (t >= TK_INTEGER && t <= TK_STRING)
        return SL_TYP;
    if (t == TK_AS || t == TK_TYPE) {
        *state = HS_AS;
        return SL_KW;
    }

    if (t == TK_SUB || t == TK_FUNCTION || t == TK_DEF) {
        *state = HS_DEF;
        return SL_KW;
    }

    if (t == TK_FN && hl.state & HS_DEF) { /* DEF FN: the name's */
        *state = HS_DEF;
        return SL_FNM;
    }

    return SL_KW;
}

/* An item other than REM and DATA at *p, past c, its first byte: its slot; *p past it,
   *state the next item's. */
static uint8_t item(const uint8_t c, const uint8_t **const p, uint8_t *const state)
{
    if (c == M_VAR) {
        *p += **p + 3; /* len name nam:2 */
        if (hl.state & HS_AS)
            return SL_TYP;
        if (hl.state & HS_DEF) {
            *state = HS_NAME;
            return SL_FNM;
        }

        return hl.depth ? SL_PAR : SL_NAM;
    }

    if (c == M_INT) {
        *p += 4;
        return SL_NUM;
    }

    if (c == '"') {
        *p = string_end(*p);
        return SL_STR;
    }

    if (c == TK_X)
        return *(*p)++ == TK1_ANY ? SL_TYP : SL_KW;
    if (c >= TK_MIN)
        return token(c, state);
    if (c == '(') { /* ( after the name: the parameters */
        if (hl.state & HS_NAME)
            hl.depth = 1;
        else if (hl.depth)
            hl.depth++;
    } else if (c == ')') {
        if (hl.depth)
            hl.depth--;
    } else if (c == '\\') {
        return SL_OP;
    } else if (c == '.' || is_digit(c)) {
        *p = number(*p);
        return SL_NUM;
    }

    return SL_ED;
}

/* hl.cols for the len characters at text. */
static void line(Session *const S, const uint8_t *const text, const uint32_t len)
{
    const uint32_t n = len > LNMAX ? LNMAX : len;
    hl.len = n;
    memmove(uimn.line, text, n); /* a copy: the lexer changes its text */
    uimn.line[n] = 0;
    hl.prev_col = 0;
    hl.prev_slot = SL_ED;
    lexr_line(S, uimn.line, S->comp.cbuf);
    const uint8_t *p = S->comp.cbuf;
    hl.state = 0;
    hl.depth = 0;

    for (;;) {
        const uint32_t off = (uint32_t)(p - S->comp.cbuf);

        if (off >= LMAX)
            break;

        const uint32_t col = S->lexr.col[off];
        const uint8_t c = *p++;

        if (!c)
            break;
        if (c == TK_REM) { /* REM, ': the rest */
            paint(col, SL_COM);
            break;
        }

        if (c == TK_DATA) { /* DATA's word; its items plain, to a ':' out of quotes */
            paint(col, SL_KW);
            paint(col + DATA_LEN, SL_ED);

            if (!(p = data_end(p)))
                break;

            continue;
        }

        uint8_t state = 0;
        const uint8_t slot = item(c, &p, &state);
        hl.state = state;
        paint(col, slot);
    }

    paint(hl.len, hl.prev_slot);
}

void synt_row(
    Session *const S,
    const uint8_t row,
    const uint8_t col,
    const uint8_t *const text,
    const uint32_t len
)
{
    if (!synt.on)
        return;

    line(S, text, len);
    const uint32_t left = uimn.ed.left;

    for (uint16_t i = 0; i < uimn.ed.width && left + i < hl.len; i++)
        if (hl.cols[i] != SL_ED)
            scrn_attr(row, (uint8_t)(col + i), 1, hl.cols[i]);
}
