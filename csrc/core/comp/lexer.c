/* The tokenizer: a typed line becomes the body of a stored line.
 *
 * # How it works
 * In the body:
 *   M_VAR len name nam:2  a name as typed, its suffix too; nam its entry in the name table
 *   M_INT n:4             a whole number under 2^31, &H, &O
 *   a token               80h..FEh, or TK_X and a byte (comp/dfa.h)
 * Spaces go, but in strings, REM and DATA. A number with a point, an exponent or 2^31 and over
 * stays text. A keyword is a whole word (the scanner's automaton, kwstep), any other word a name.
 * FN is joined to its name; E and D of an exponent go upper case.
 *
 * Lexr.col: an item's column in the text, by its body offset (the editor's). A streamed text is
 * read through a window: its LexrSource refills it. Names come from a LexrNames. This file also
 * defines dfa.h's tables (DFA_DATA).
 *
 * # Example
 *    x = 5 + y     M_VAR 1 "x" nam, TK_EQ, M_INT 5, TK_PLUS, M_VAR 1 "y" nam
 *
 * # asm structure (src/comp/lex.asm)
 * | asm | here | |
 * |---|---|---|
 * | lx_line | lexr_line | MIRROR; past the body's 0; D3 |
 * | lx_kw | lexr_keyword(&at) | MIRROR; token, 0 a name |
 * | lx_colof | lexr_colof(off) | MIRROR; the column |
 * | lx_item, lx_more, lx_ahead, lx_col, lx_fn, lx_num, lx_hex, lx_word, kw_step, lx_name | lexr_line's loop, lexr_more, lexr_ahead, lexr_markcol, lexr_joinfn, lexr_number, lexr_hexnum, lexr_word, lexr_kwstep, lexr_name | MIRROR |
 * | g_lxcol, g_lxall, g_lxfill | Lexr.col, Lexr.all, Lexr.source | D3 |
 * | g_lxlst, g_lxend, g_lxlc | lexr.last, lexr.end, lexr.lastcol | MIRROR |
 * | lx.lxsrc, lx.lxdst | lexr.src, lexr.dst | MIRROR |
 * | lx.nsyms, lx.nnams | symb.nvars, symb.nnames (mem's) | MOVED |
 * | lx.dproc, lx.ntyps, lx.nflds, lx.dtype | decl.proc, decl.ntypes, decl.nfields, decl.type (declare.c) | MOVED |
 * | lx.lxf, lx.lxn, lx.lxs | locals | MIRROR |
 * | lx.nlen, lx.nptr | symb_find_name's parameters | D1 |
 * | lx.lxst, lx.nkey, lx.nlenp, lx.lxv, lx.lxm | | D2 |
 *
 * - D1. The name's length and pointer are parameters, not globals.
 * - D2. Unused in the asm; dropped.
 * - D3. It takes its own state; names, streamed text and errors come through LexrNames,
 *   LexrSource and Fault, which the session provides.
 */
#define DFA_DATA
#include "comp/lexer.h"
#include "util/grow.h"
#include "comp/dfa.h"
#include "num/single.h"

enum { LXAHEAD = 128 }; /* ahead: a word's, a number's bytes in the window */

[[noreturn]]
static void lexr_fail(const Lexr *const lx)
{
    fault_error(lx->fault, E_OM);
}

void lexr_init(Lexr *const lx, Fault *const f, LexrNames *const names)
{
    lx->fault = f;
    lx->names = names;
}

static void lexr_room(Lexr *const lx)
{
    if (lx->out >= lx->end)
        lexr_fail(lx);
}

/* Streamed text: the window's end at src; the window slid, its bytes from src on kept, and read
   on. False: the line has no more. */
static bool lexr_more(Lexr *const lx)
{
    if (!lx->source)
        return false;

    const uint8_t *keep = lx->in;

    if (!lx->source->more(lx->source, &keep))
        return false;

    const ptrdiff_t moved = lx->in - keep;
    lx->src -= moved;
    lx->word -= moved;
    lx->in = (uint8_t *)keep;
    return true;
}

/* Streamed: the item at src-1 and LXAHEAD bytes after it in the window. */
static void lexr_ahead(Lexr *const lx)
{
    if (lx->source && lx->source->end - lx->in < LXAHEAD) {
        lx->in--;
        lexr_more(lx);
        lx->in++;
    }
}

/* The item at src-1: its column to lexr.col at the body's offset. */
static void lexr_markcol(Lexr *const lx)
{
    const uint32_t off = (uint32_t)(lx->out - lx->dst);

    if (off >= LMAX)
        lexr_fail(lx);
    if (!lx->cols)
        return;
    if (off >= lx->ncol && !GROW(lx->col, lx->ncol, off + 1))
        lexr_fail(lx);
    if (lx->all)
        for (int32_t i = lx->last + 1; i < (int32_t)off; i++)
            lx->col[i] = lx->lastcol;

    const uint16_t col = (uint16_t)(lx->in - 1 - lx->src);
    lx->col[off] = col;
    lx->lastcol = col;
    lx->last = (int32_t)off;
}

uint16_t lexr_colof(const Lexr *const lx, uint32_t off)
{
    if (lx->last < 0)
        return 0;
    if (off > (uint32_t)lx->last)
        off = (uint32_t)lx->last;

    return lx->col[off];
}

/* DEF FN name, FN name: FN and the name after its blanks made one name, the blanks moved before. */
static void lexr_joinfn(Lexr *const lx)
{
    if (upcase(lx->in[0]) != 'F' || upcase(lx->in[1]) != 'N' || !is_blank(lx->in[2]))
        return;

    uint8_t *b = lx->in + 2;

    while (is_blank(*b))
        b++;
    if (!is_letter(*b))
        return;

    const uint8_t f = lx->in[0];
    const uint8_t n = lx->in[1];
    lx->in[0] = lx->in[1] = ' '; /* blanks first: one may overlap */
    b[-2] = f;
    b[-1] = n;
    lx->in = b - 2;
}

/* A number: M_INT n:4 when whole and under 2^31, else its text. */
static void lexr_number(Lexr *const lx)
{
    uint8_t *const start = lx->out;
    int64_t v = 0;
    bool text = false;

    for (;;) {
        lexr_room(lx);

        if (*lx->in == 0)
            lexr_more(lx);

        const uint8_t c = *lx->in;

        if (is_digit(c)) {
            *lx->out++ = *lx->in++;

            if (text)
                continue;

            v = v * 10 + (c - '0');

            if (v > INT32_MAX)
                text = true; /* 2^31 and over */
            continue;
        }

        if (c == '.') {
            *lx->out++ = *lx->in++;
            text = true;
            continue;
        }

        const uint8_t u = upcase(c);

        if (u == 'E' || u == 'D') { /* an exponent when a digit or sign follows */
            const uint8_t b = lx->in[1];

            if (b == '+' || b == '-') {
                *lx->in = u;
                *lx->out++ = *lx->in++;
                *lx->out++ = *lx->in++;
                text = true;
                continue;
            }

            if (is_digit(b)) {
                *lx->in = u;
                *lx->out++ = *lx->in++;
                text = true;
                continue;
            }
        }

        break;
    }

    if (*lx->in == '#' || *lx->in == '!') { /* a DOUBLE's, a SINGLE's */
        *lx->out++ = *lx->in++;
        text = true;
    }

    if (text)
        return;
    if (*lx->in == '%' || *lx->in == '&') /* an INTEGER's, a LONG's: one type */
        lx->in++;

    lx->out = start;
    *lx->out++ = M_INT;
    store32(lx->out, (uint32_t)v);
    lx->out += 4;
}

/* &H or &O, its digits, a % or & after: M_INT n:4; '&' alone is the byte. src after '&'. */
static void lexr_hexnum(Lexr *const lx)
{
    uint32_t v;
    const uint8_t *p = lx->in;

    if (!sngl_hexoct(&p, &v)) {
        *lx->out++ = '&';
        return;
    }

    lx->in = (uint8_t *)p;

    if (*lx->in == '%' || *lx->in == '&')
        lx->in++;

    *lx->out++ = M_INT;
    store32(lx->out, v);
    lx->out += 4;
}

static uint16_t lexr_kwstep(const uint16_t st, const uint8_t cls)
{
    for (uint32_t i = lexr_kw_first[st]; i < lexr_kw_first[st + 1]; i++)
        if (lexr_kw_char[i] == cls)
            return lexr_kw_next[i];

    return 0;
}

uint16_t lexr_keyword(const uint8_t **const at)
{
    const uint8_t *p = *at;
    uint16_t st = KW_START;

    while (is_letter(*p) || is_digit(*p) || *p == '.')
        st = lexr_kwstep(st, lexr_kw_class[upcase(*p++)]);
    if (*p == '$') { /* a '$' when the keyword has one */
        const uint16_t s2 = lexr_kwstep(st, lexr_kw_class['$']);

        if (lexr_kw_token[s2]) {
            st = s2;
            p++;
        }
    }

    *at = p;
    return lexr_kw_token[st];
}

/* The word from wstart to src is a name: M_VAR len name nam:2, a suffix in the text but not the
   name's key. */
static void lexr_name(Lexr *const lx)
{
    *lx->out++ = M_VAR;
    uint8_t *const len = lx->out++;
    uint8_t *const text = lx->out;
    const uint8_t n = (uint8_t)(lx->in - lx->word);
    memcpy(lx->out, lx->word, n);
    lx->out += n;
    const uint8_t c = *lx->in;

    if (c == '%' || c == '&' || c == '!' || c == '#' || c == '$')
        *lx->out++ = *lx->in++;

    *len = (uint8_t)(lx->out - text);
    const uint16_t id = lx->names->intern(lx->names, text, n);
    store16(lx->out, id);
    lx->out += 2;
}

/* A word: its keyword's token written, else a name; the token, 0 a name. */
static uint8_t lexr_word(Lexr *const lx)
{
    lx->word = lx->in;
    const uint8_t *p = lx->in;
    const uint16_t t = lexr_keyword(&p);
    lx->in = (uint8_t *)p;

    if (!t) {
        lexr_name(lx);
        return 0;
    }

    if (t >= 0x100)
        *lx->out++ = TK_X;

    *lx->out++ = (uint8_t)t;
    return (uint8_t)t;
}

/* The line's end: its column, its 0. */
static uint8_t *lexr_finish(Lexr *const lx)
{
    lexr_markcol(lx);
    *lx->out++ = 0;
    return lx->out;
}

/* The next byte of the text, the window refilled first, src past it: 0 at the line's end, src
   past the 0 too (its column is the end's). */
static uint8_t lexr_next_char(Lexr *const lx)
{
    while (!*lx->in && lexr_more(lx))
        ;

    return *lx->in++;
}

/* A string's rest, to its quote, as it is. false: the line ended. */
static bool lexr_copy_string(Lexr *const lx)
{
    for (;;) {
        lexr_room(lx);
        const uint8_t c = lexr_next_char(lx);

        if (!c)
            return false;

        *lx->out++ = c;

        if (c == '"')
            return true;
    }
}

/* DATA's text: to a ':' out of quotes. false: the line ended. */
static bool lexr_copy_data(Lexr *const lx)
{
    bool quoted = false;

    for (;;) {
        lexr_room(lx);
        const uint8_t c = lexr_next_char(lx);

        if (!c)
            return false;
        if (c == '"')
            quoted = !quoted;

        *lx->out++ = c;

        if (!quoted && c == ':')
            return true;
    }
}

/* REM's text: the rest, as it is, then the 0. */
static uint8_t *lexr_copy_rest(Lexr *const lx)
{
    for (;;) {
        lexr_room(lx);
        const uint8_t c = lexr_next_char(lx);
        *lx->out++ = c;

        if (!c)
            return lx->out;
    }
}

uint8_t *lexr_line(Lexr *const lx, uint8_t *const text, uint8_t *const body)
{
    lx->in = text;
    lx->out = body;
    lx->src = text;
    lx->dst = body;
    lx->end = body + LMAX;
    lx->last = -1;

    for (;;) {
        const uint8_t c = lexr_next_char(lx);

        if (!c)
            return lexr_finish(lx);
        if (is_blank(c))
            continue;

        lexr_ahead(lx);
        lexr_markcol(lx);

        if (c == '"') {
            *lx->out++ = c;

            if (!lexr_copy_string(lx))
                return lexr_finish(lx);
        } else if (c == '\'') {
            *lx->out++ = TK_REM;
            *lx->out++ = REM_Q;
            return lexr_copy_rest(lx);
        } else if (c == '&')
            lexr_hexnum(lx);
        else if (c == '.' && !is_digit(*lx->in)) /* a '.' alone */
            *lx->out++ = c;
        else if (c == '.' || is_digit(c)) {
            lx->in--;
            lexr_number(lx);
        } else if (is_letter(c)) {
            lx->in--;
            lexr_joinfn(lx);
            const uint8_t t = lexr_word(lx);

            if (t == TK_REM)
                return lexr_copy_rest(lx);
            if (t == TK_DATA && !lexr_copy_data(lx))
                return lexr_finish(lx);
        } else {
            const uint8_t t = lexr_char_token[c];
            *lx->out++ = t ? t : c;
        }
    }
}
