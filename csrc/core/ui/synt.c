/* Syntax highlighting (src/ui/syntax.asm). The line's body, each item at its column
   (lexr.col), gives its slot to the columns from there to the next item's. state: what the last
   item makes of the next: after AS or TYPE a type; after SUB, FUNCTION, DEF a name being
   declared, then its parameters in the ( ) after it, depth deep. */
#include "ui/synt.h"
#include "ui/scrn.h"
#include "comp/comp.h"
#include "comp/dfa.h"
#include "comp/lexr.h"

Synt synt = {.on = true};

enum : uint8_t { HS_AS = 1, HS_DEF = 2, HS_NAME = 4 };

static struct {
    uint8_t state, depth;
    uint32_t len;
    uint8_t prev_slot;  /* the last item's slot */
    uint32_t prev_col;  /* its column */
    uint8_t cols[256];  /* the slots of the columns shown */
} hl;

void synt_toggle(void) { synt.on = !synt.on; }

/* slot from column col on; the item before it to col in its own, the columns shown only. */
static void paint(uint32_t col, uint8_t slot)
{
    int32_t left = uimn.ed.left, from = (int32_t)hl.prev_col, to = left + uimn.ed.width;
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
static const uint8_t *number(const uint8_t *p)
{
    for (;; p++) {
        uint8_t c = *p;
        if (!(c >= '0' && c <= '9') && c != '.' && c != 'E' && c != 'D' && c != '+' &&
            c != '-' && c != '#' && c != '!')
            return p;
    }
}

/* Past DATA's items' ':' out of quotes; nullptr: the body's end. */
static const uint8_t *data_end(const uint8_t *p)
{
    bool quoted = false;
    for (uint8_t c; (c = *p++);) {
        if (c == '"')
            quoted = !quoted;
        if (!quoted && c == ':')
            return p;
    }
    return nullptr;
}

/* A token's slot (80h .. FEh); *state the next item's. */
static uint8_t token(uint8_t t, uint8_t *state)
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

/* hl.cols for the len characters at text. */
static void line(const uint8_t *text, uint32_t len)
{
    if (len > LNMAX)
        len = LNMAX;
    hl.len = len;
    memmove(uimn.line, text, len); /* a copy: the lexer changes its text */
    uimn.line[len] = 0;
    hl.prev_col = 0;
    hl.prev_slot = SL_ED;
    lexr_line(uimn.line, comp_cbuf);
    const uint8_t *p = comp_cbuf;
    hl.state = 0;
    hl.depth = 0;
    for (;;) {
        uint32_t off = (uint32_t)(p - comp_cbuf);
        if (off >= LMAX)
            break;
        uint32_t col = lexr.col[off];
        uint8_t c = *p++, slot = SL_ED, state = 0;
        if (!c)
            break;
        if (c == M_VAR) {
            p += *p + 3; /* len name nam:2 */
            if (hl.state & HS_AS)
                slot = SL_TYP;
            else if (hl.state & HS_DEF) {
                slot = SL_FNM;
                state = HS_NAME;
            } else
                slot = hl.depth ? SL_PAR : SL_NAM;
        } else if (c == M_INT) {
            p += 4;
            slot = SL_NUM;
        } else if (c == '"') {
            while (*p && *p++ != '"')
                ;
            slot = SL_STR;
        } else if (c == TK_REM) { /* REM, ': the rest */
            paint(col, SL_COM);
            break;
        } else if (c == TK_DATA) { /* DATA's word; its items plain, to a ':' out of quotes */
            paint(col, SL_KW);
            paint(col + 4, SL_ED);
            if (!(p = data_end(p)))
                break;
            continue;
        } else if (c == TK_X)
            slot = *p++ == TK1_ANY ? SL_TYP : SL_KW;
        else if (c >= 0x80)
            slot = token(c, &state);
        else if (c == '(') { /* ( after the name: the parameters */
            if (hl.state & HS_NAME)
                hl.depth = 1;
            else if (hl.depth)
                hl.depth++;
        } else if (c == ')') {
            if (hl.depth)
                hl.depth--;
        } else if (c == '\\')
            slot = SL_OP;
        else if (c == '.' || (c >= '0' && c <= '9')) {
            p = number(p);
            slot = SL_NUM;
        }
        hl.state = state;
        paint(col, slot);
    }
    paint(hl.len, hl.prev_slot);
}

void synt_row(uint8_t row, uint8_t col, const uint8_t *text, uint32_t len)
{
    if (!synt.on)
        return;
    line(text, len);
    uint32_t c = uimn.ed.left;
    for (uint16_t i = 0; i < uimn.ed.width && c < hl.len; i++, c++, col++)
        if (hl.cols[i] != SL_ED)
            scrn_attr(row, col, 1, hl.cols[i]);
}
