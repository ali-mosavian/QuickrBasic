/* The tokenizer (comp/lex.asm). */
#define DFA_DATA
#include "comp/lexr.h"
#include "session.h"
#include "comp/dfa.h"
#include "io/repl.h"
#include "io/srcr.h"
#include "mem/symb.h"
#include "num/sngl.h"


enum { LXAHEAD = 128 }; /* ahead: a word's, a number's bytes in the window */


static void room(Session *const S)
{
    if (S->lexr.out >= S->lexr.end)
        repl_error(S, E_OM);
}

/* Streamed text: the window's end at src; the window slid, its bytes from src on kept, and read
   on. False: the line has no more. */
static bool more(Session *const S)
{
    if (!S->lexr.more)
        return false;
    const uint8_t *keep = S->lexr.in;
    if (!S->lexr.more(S, &keep))
        return false;
    const ptrdiff_t moved = S->lexr.in - keep;
    S->lexr.src -= moved;
    S->lexr.word -= moved;
    S->lexr.in = (uint8_t *)keep;
    return true;
}

/* Streamed: the item at src-1 and LXAHEAD bytes after it in the window. */
static void ahead(Session *const S)
{
    if (S->lexr.more && S->srcr.end - S->lexr.in < LXAHEAD) {
        S->lexr.in--;
        more(S);
        S->lexr.in++;
    }
}

/* The item at src-1: its column to lexr.col at the body's offset. */
static void markcol(Session *const S)
{
    const uint32_t off = (uint32_t)(S->lexr.out - S->lexr.dst);
    if (off >= LMAX)
        repl_error(S, E_OM);
    if (off >= S->lexr.ncol)
        return;
    if (S->lexr.all)
        for (int32_t i = S->lexr.last + 1; i < (int32_t)off; i++)
            S->lexr.col[i] = S->lexr.lastcol;
    const uint16_t col = (uint16_t)(S->lexr.in - 1 - S->lexr.src);
    S->lexr.col[off] = col;
    S->lexr.lastcol = col;
    S->lexr.last = (int32_t)off;
}

uint16_t lexr_colof(Session *const S, uint32_t off)
{
    if (S->lexr.last < 0)
        return 0;
    if (off > (uint32_t)S->lexr.last)
        off = (uint32_t)S->lexr.last;
    return S->lexr.col[off];
}

/* DEF FN name, FN name: FN and the name after its blanks made one name, the blanks moved before. */
static void joinfn(Session *const S)
{
    if (upcase(S->lexr.in[0]) != 'F' || upcase(S->lexr.in[1]) != 'N' || !is_blank(S->lexr.in[2]))
        return;
    uint8_t *b = S->lexr.in + 2;
    while (is_blank(*b))
        b++;
    if (!is_letter(*b))
        return;
    const uint8_t f = S->lexr.in[0];
    const uint8_t n = S->lexr.in[1];
    S->lexr.in[0] = S->lexr.in[1] = ' '; /* blanks first: one may overlap */
    b[-2] = f;
    b[-1] = n;
    S->lexr.in = b - 2;
}

/* A number: M_INT n:4 when whole and under 2^31, else its text. */
static void number(Session *const S)
{
    uint8_t *const start = S->lexr.out;
    int64_t v = 0;
    bool text = false;
    for (;;) {
        room(S);
        if (*S->lexr.in == 0)
            more(S);
        const uint8_t c = *S->lexr.in;
        if (is_digit(c)) {
            *S->lexr.out++ = *S->lexr.in++;
            if (text)
                continue;
            v = v * 10 + (c - '0');
            if (v > INT32_MAX)
                text = true; /* 2^31 and over */
            continue;
        }
        if (c == '.') {
            *S->lexr.out++ = *S->lexr.in++;
            text = true;
            continue;
        }
        const uint8_t u = upcase(c);
        if (u == 'E' || u == 'D') { /* an exponent when a digit or sign follows */
            const uint8_t b = S->lexr.in[1];
            if (b == '+' || b == '-') {
                *S->lexr.in = u;
                *S->lexr.out++ = *S->lexr.in++;
                *S->lexr.out++ = *S->lexr.in++;
                text = true;
                continue;
            }
            if (is_digit(b)) {
                *S->lexr.in = u;
                *S->lexr.out++ = *S->lexr.in++;
                text = true;
                continue;
            }
        }
        break;
    }
    if (*S->lexr.in == '#' || *S->lexr.in == '!') { /* a DOUBLE's, a SINGLE's */
        *S->lexr.out++ = *S->lexr.in++;
        text = true;
    }
    if (text)
        return;
    if (*S->lexr.in == '%' || *S->lexr.in == '&') /* an INTEGER's, a LONG's: one type */
        S->lexr.in++;
    S->lexr.out = start;
    *S->lexr.out++ = M_INT;
    store32(S->lexr.out, (uint32_t)v);
    S->lexr.out += 4;
}

/* &H or &O, its digits, a % or & after: M_INT n:4; '&' alone is the byte. src after '&'. */
static void hexnum(Session *const S)
{
    uint32_t v;
    const uint8_t *p = S->lexr.in;
    if (!sngl_hexoct(&p, &v)) {
        *S->lexr.out++ = '&';
        return;
    }
    S->lexr.in = (uint8_t *)p;
    if (*S->lexr.in == '%' || *S->lexr.in == '&')
        S->lexr.in++;
    *S->lexr.out++ = M_INT;
    store32(S->lexr.out, v);
    S->lexr.out += 4;
}

static uint16_t kwstep(const uint16_t st, const uint8_t cls)
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
        st = kwstep(st, lexr_kw_class[upcase(*p++)]);
    if (*p == '$') { /* a '$' when the keyword has one */
        const uint16_t s2 = kwstep(st, lexr_kw_class['$']);
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
static void name(Session *const S)
{
    *S->lexr.out++ = M_VAR;
    uint8_t *const len = S->lexr.out++;
    uint8_t *const text = S->lexr.out;
    const uint8_t n = (uint8_t)(S->lexr.in - S->lexr.word);
    memcpy(S->lexr.out, S->lexr.word, n);
    S->lexr.out += n;
    const uint8_t c = *S->lexr.in;
    if (c == '%' || c == '&' || c == '!' || c == '#' || c == '$')
        *S->lexr.out++ = *S->lexr.in++;
    *len = (uint8_t)(S->lexr.out - text);
    const uint16_t id = symb_find_name(S, text, n);
    store16(S->lexr.out, id);
    S->lexr.out += 2;
}

/* A word: its keyword's token written, else a name; the token, 0 a name. */
static uint8_t word(Session *const S)
{
    S->lexr.word = S->lexr.in;
    const uint8_t *p = S->lexr.in;
    const uint16_t t = lexr_keyword(&p);
    S->lexr.in = (uint8_t *)p;
    if (!t) {
        name(S);
        return 0;
    }
    if (t >= 0x100)
        *S->lexr.out++ = TK_X;
    *S->lexr.out++ = (uint8_t)t;
    return (uint8_t)t;
}

/* The line's end: its column, its 0. */
static uint8_t *finish(Session *const S)
{
    markcol(S);
    *S->lexr.out++ = 0;
    return S->lexr.out;
}

/* The next byte of the text, the window refilled first, src past it: 0 at the line's end, src
   past the 0 too (its column is the end's). */
static uint8_t next_char(Session *const S)
{
    while (!*S->lexr.in && more(S))
        ;
    return *S->lexr.in++;
}

/* A string's rest, to its quote, as it is. false: the line ended. */
static bool copy_string(Session *const S)
{
    for (;;) {
        room(S);
        const uint8_t c = next_char(S);
        if (!c)
            return false;
        *S->lexr.out++ = c;
        if (c == '"')
            return true;
    }
}

/* DATA's text: to a ':' out of quotes. false: the line ended. */
static bool copy_data(Session *const S)
{
    bool quoted = false;
    for (;;) {
        room(S);
        const uint8_t c = next_char(S);
        if (!c)
            return false;
        if (c == '"')
            quoted = !quoted;
        *S->lexr.out++ = c;
        if (!quoted && c == ':')
            return true;
    }
}

/* REM's text: the rest, as it is, then the 0. */
static uint8_t *copy_rest(Session *const S)
{
    for (;;) {
        room(S);
        const uint8_t c = next_char(S);
        *S->lexr.out++ = c;
        if (!c)
            return S->lexr.out;
    }
}


uint8_t *lexr_line(Session *const S, uint8_t *const text, uint8_t *const body)
{
    S->lexr.in = text;
    S->lexr.out = body;
    S->lexr.src = text;
    S->lexr.dst = body;
    S->lexr.end = body + LMAX;
    S->lexr.last = -1;
    for (;;) {
        const uint8_t c = next_char(S);
        if (!c)
            return finish(S);
        if (is_blank(c))
            continue;
        ahead(S);
        markcol(S);
        if (c == '"') {
            *S->lexr.out++ = c;
            if (!copy_string(S))
                return finish(S);
        } else if (c == '\'') {
            *S->lexr.out++ = TK_REM;
            *S->lexr.out++ = REM_Q;
            return copy_rest(S);
        } else if (c == '&')
            hexnum(S);
        else if (c == '.' && !is_digit(*S->lexr.in)) /* a '.' alone */
            *S->lexr.out++ = c;
        else if (c == '.' || is_digit(c)) {
            S->lexr.in--;
            number(S);
        } else if (is_letter(c)) {
            S->lexr.in--;
            joinfn(S);
            const uint8_t t = word(S);
            if (t == TK_REM)
                return copy_rest(S);
            if (t == TK_DATA && !copy_data(S))
                return finish(S);
        } else {
            const uint8_t t = lexr_char_token[c];
            *S->lexr.out++ = t ? t : c;
        }
    }
}
