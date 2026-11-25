/* Syntax highlighting: a line's keywords, strings, numbers, comments and names in the theme's
 * colours.
 *
 * # How it works
 * synt_row tokenizes the line as the compiler does (lexr_line with lexr.col). Each item of the
 * body, at its column, gives its slot to the columns from there to the next item's. The columns
 * SL_ED has are left as drawn. Off (synt.on false): nothing.
 *
 * A state carries what the last item makes of the next: after AS or TYPE a type; after SUB,
 * FUNCTION, DEF a name being declared, then its parameters in the ( ) after it, depth deep.
 *
 * The text is copied to uimn.line first: the lexer changes it. A text dialect's program's lines
 * are coloured by the dialect (Dialect.colour), each part's colour a slot.
 *
 * # Example
 *   FOR i = 1 TO 10 ' loop    FOR, TO: SL_KW; i: SL_NAM; 1, 10: SL_NUM; ' loop: SL_COM
 *
 * # asm structure (retired/asm32/ui/synt.asm)
 * | asm | here | |
 * |---|---|---|
 * | hl_toggle | synt_toggle | MIRROR |
 * | hl_row | synt_row | MIRROR |
 * | g_hlon | synt.on (starts true) | MIRROR |
 * | hl_line, g_hls, g_hld, g_hlen, g_hlpa, g_hlpc, g_hlc, HS_* | line and its statics | MIRROR |
 */
#include "ui/highlight.h"
#include "session.h"
#include "ui/edit.h"
#include "ui/screen.h"
#include "comp/comp.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "comp/lexer.h"
#include "ui/text.h"

Synt synt = {.on = true};

enum : uint8_t { HS_AS = 1, HS_DEF = 2, HS_NAME = 4 };
enum : uint8_t { TK_MIN = 0x80 }; /* a token's first byte from here */
enum { DATA_LEN = 4 };            /* "DATA" */

static struct {
    uint8_t state, depth;
    uint32_t len;
    uint8_t prev_slot; /* the last item's slot */
    uint32_t prev_col; /* its column */
    uint32_t left;     /* the columns worked out: width from left */
    uint16_t width;
    uint8_t cols[SYNT_KEEP]; /* their slots */
} hl;

void synt_toggle(void) { synt.on = !synt.on; }

/* slot from column col on; the item before it to col in its own, the columns shown only. */
static void paint(const uint32_t col, const uint8_t slot)
{
    const int32_t left = (int32_t)hl.left;
    int32_t from = (int32_t)hl.prev_col, to = left + hl.width;

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
        *p += 1 + **p + NAME_ID; /* len name nam */
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

    if (c == TK_X) { /* ANY, SHORT, BYTE: a type */
        const uint8_t t = *(*p)++;
        return t == TK1_ANY || decl_xastype(t) ? SL_TYP : SL_KW;
    }

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

/* A text dialect's colour's slot. */
static const uint8_t text_slots[] = {
    [DHL_PLAIN] = SL_ED,
    [DHL_KW] = SL_KW,
    [DHL_NAME] = SL_NAM,
    [DHL_TYPE] = SL_TYP,
    [DHL_FN] = SL_FNM,
    [DHL_PARAM] = SL_PAR,
    [DHL_NUM] = SL_NUM,
    [DHL_STR] = SL_STR,
    [DHL_OP] = SL_OP,
    [DHL_COMMENT] = SL_COM,
};

/* Dialect.colour's: colour c from column col on. */
static void text_part(void *const ctx, const uint32_t col, const DialColour c)
{
    paint(col, text_slots[c]);
}

/* hl.cols for a text dialect's line, the n characters at text: its parts as d colours them. */
static void text_line(
    Session *const S,
    const Dialect *const d,
    const uint8_t *const text,
    const uint32_t n
)
{
    d->colour(&S->fault, (const char *)text, n, text_part, nullptr);
    paint(n, hl.prev_slot);
}

/* hl.cols for the len characters at text, dialect d's, width columns from left. */
static void line(
    Session *const S,
    const Dialect *const d,
    const uint8_t *const text,
    const uint32_t len,
    const uint32_t left,
    const uint16_t width
)
{
    hl.left = left;
    hl.width = width;
    const uint32_t n = len > LNMAX ? LNMAX : len;
    hl.len = n;
    uint8_t *const line = edit_lb_need(S, &uimn.line, n);
    memmove(line, text, n); /* a copy: the lexer changes its text */
    line[n] = 0;
    hl.prev_col = 0;
    hl.prev_slot = SL_ED;
    hl.state = 0;
    hl.depth = 0;

    if (d->text) {
        text_line(S, d, line, n);
        return;
    }

    lexr_line(&S->lexr, line, S->comp.cbuf);
    const uint8_t *p = S->comp.cbuf;

    for (;;) {
        const uint32_t off = (uint32_t)(p - S->comp.cbuf);

        if (off >= LMAX)
            break;

        const uint32_t col = lexr_colof(&S->lexr, off);
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
    const Dialect *const d,
    const uint8_t row,
    const uint8_t col,
    const uint8_t *const text,
    const uint32_t len
)
{
    if (!synt.on)
        return;

    const uint32_t left = uimn.ed.left;
    line(S, d, text, len, left, uimn.ed.width);

    for (uint16_t i = 0; i < uimn.ed.width && left + i < hl.len; i++)
        if (hl.cols[i] != SL_ED)
            scrn_attr(row, (uint8_t)(col + i), 1, hl.cols[i]);
}

void synt_cols(
    Session *const S,
    const Dialect *const d,
    const uint8_t *const text,
    const uint32_t len,
    uint8_t cols[static SYNT_KEEP]
)
{
    line(S, d, text, len, 0, SYNT_KEEP);
    memcpy(cols, hl.cols, len);
}

const uint8_t *synt_need(
    Session *const S,
    const Dialect *const d,
    const uint8_t *const text,
    const uint32_t len,
    const uint8_t *const cols
)
{
    if (cols)
        return cols;

    line(S, d, text, len, 0, SYNT_KEEP);
    return hl.cols;
}

static bool suffix(const uint8_t c) { return c && memchr("%&!#$", c, 5) != nullptr; }

static bool name_slot(const uint8_t s) { return s == SL_NAM || s == SL_PAR || s == SL_FNM; }

bool synt_name(
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

void synt_paint(const uint8_t row, const uint8_t col, const uint8_t *const cols, const uint32_t len)
{
    const uint32_t left = uimn.ed.left;

    for (uint16_t i = 0; i < uimn.ed.width && left + i < len; i++)
        if (cols[left + i] != SL_ED)
            scrn_attr(row, (uint8_t)(col + i), 1, cols[left + i]);
}
