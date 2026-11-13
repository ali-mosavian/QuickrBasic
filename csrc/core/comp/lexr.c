/* The tokenizer (comp/lex.asm). */
#define DFA_DATA
#include "comp/lexr.h"
#include "comp/dfa.h"
#include "io/repl.h"
#include "io/srcr.h"
#include "mem/symb.h"
#include "num/sngl.h"

Lexr lexr;

enum { LXAHEAD = 128 }; /* ahead: a word's, a number's bytes in the window */

static uint8_t *src;          /* (ESI) the text */
static uint8_t *dst;          /* (EDI) the body */
static const uint8_t *wstart; /* lx.lxs: a word's start */

static uint8_t upcase(uint8_t c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
static bool isletter(uint8_t c) { return (uint8_t)(upcase(c) - 'A') < 26; }
static bool isdigit_(uint8_t c) { return (uint8_t)(c - '0') < 10; }

static void room(void)
{
    if (dst >= lexr.end)
        repl_error(E_OM);
}

/* Streamed text: the window's end at src; the window slid, its bytes from src on kept, and read
   on. False: the line has no more. */
static bool more(void)
{
    if (!lexr.more)
        return false;
    const uint8_t *keep = src;
    if (!lexr.more(&keep))
        return false;
    ptrdiff_t moved = src - keep;
    lexr.src -= moved;
    wstart -= moved;
    src = (uint8_t *)keep;
    return true;
}

/* Streamed: the item at src-1 and LXAHEAD bytes after it in the window. */
static void ahead(void)
{
    if (lexr.more && srcr.end - src < LXAHEAD) {
        src--;
        more();
        src++;
    }
}

/* The item at src-1: its column to lexr.col at the body's offset. */
static void markcol(void)
{
    uint32_t off = (uint32_t)(dst - lexr.dst);
    if (off >= LMAX)
        repl_error(E_OM);
    if (off >= LXCMAX)
        return;
    if (lexr.all)
        for (int32_t i = lexr.last + 1; i < (int32_t)off; i++)
            lexr.col[i] = lexr.lastcol;
    uint16_t col = (uint16_t)(src - 1 - lexr.src);
    lexr.col[off] = col;
    lexr.lastcol = col;
    lexr.last = (int32_t)off;
}

#if IDE
uint16_t lexr_colof(uint32_t off)
{
    if (lexr.last < 0)
        return 0;
    if (off > (uint32_t)lexr.last)
        off = (uint32_t)lexr.last;
    return lexr.col[off];
}
#endif

/* DEF FN name, FN name: FN and the name after its blanks made one name, the blanks moved before. */
static void joinfn(void)
{
    if ((src[0] & 0xDF) != 'F' || (src[1] & 0xDF) != 'N' || (src[2] != ' ' && src[2] != 9))
        return;
    uint8_t *b = src + 2;
    while (*b == ' ' || *b == 9)
        b++;
    if (!isletter(*b))
        return;
    uint8_t f = src[0], n = src[1];
    src[0] = src[1] = ' '; /* blanks first: one may overlap */
    b[-2] = f;
    b[-1] = n;
    src = b - 2;
}

/* A number: M_INT n:4 when whole and under 2^31, else its text. */
static void number(void)
{
    uint8_t *start = dst;
    int64_t v = 0;
    bool text = false;
    for (;;) {
        room();
        if (*src == 0)
            more();
        uint8_t c = *src;
        if (isdigit_(c)) {
            *dst++ = *src++;
            if (text)
                continue;
            v = v * 10 + (c - '0');
            if (v > INT32_MAX)
                text = true; /* 2^31 and over */
            continue;
        }
        if (c == '.') {
            *dst++ = *src++;
            text = true;
            continue;
        }
        uint8_t u = upcase(c);
        if (u == 'E' || u == 'D') { /* an exponent when a digit or sign follows */
            uint8_t b = src[1];
            if (b == '+' || b == '-') {
                *src = u;
                *dst++ = *src++;
                *dst++ = *src++;
                text = true;
                continue;
            }
            if (isdigit_(b)) {
                *src = u;
                *dst++ = *src++;
                text = true;
                continue;
            }
        }
        break;
    }
    if (*src == '#' || *src == '!') { /* a DOUBLE's, a SINGLE's */
        *dst++ = *src++;
        text = true;
    }
    if (text)
        return;
    if (*src == '%' || *src == '&') /* an INTEGER's, a LONG's: one type */
        src++;
    dst = start;
    *dst++ = M_INT;
    uint32_t n = (uint32_t)v;
    memcpy(dst, &n, 4);
    dst += 4;
}

/* &H or &O, its digits, a % or & after: M_INT n:4; '&' alone is the byte. src after '&'. */
static void hexnum(void)
{
    uint32_t v;
    const uint8_t *p = src;
    if (!sngl_hexoct(&p, &v)) {
        *dst++ = '&';
        return;
    }
    src = (uint8_t *)p;
    if (*src == '%' || *src == '&')
        src++;
    *dst++ = M_INT;
    memcpy(dst, &v, 4);
    dst += 4;
}

static uint16_t kwstep(uint16_t st, uint8_t cls)
{
    for (uint32_t i = lexr_kw_first[st]; i < lexr_kw_first[st + 1]; i++)
        if (lexr_kw_char[i] == cls)
            return lexr_kw_next[i];
    return 0;
}

uint16_t lexr_keyword(const uint8_t **at)
{
    const uint8_t *p = *at;
    uint16_t st = KW_START;
    while (isletter(*p) || isdigit_(*p) || *p == '.')
        st = kwstep(st, lexr_kw_class[upcase(*p++)]);
    if (*p == '$') { /* a '$' when the keyword has one */
        uint16_t s2 = kwstep(st, lexr_kw_class['$']);
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
static void name(void)
{
    *dst++ = M_VAR;
    uint8_t *len = dst++;
    uint8_t *text = dst;
    uint8_t n = (uint8_t)(src - wstart);
    memcpy(dst, wstart, n);
    dst += n;
    uint8_t c = *src;
    if (c == '%' || c == '&' || c == '!' || c == '#' || c == '$')
        *dst++ = *src++;
    *len = (uint8_t)(dst - text);
    uint16_t id = symb_find_name(text, n);
    memcpy(dst, &id, 2);
    dst += 2;
}

/* A word: its keyword's token written, else a name; the token, 0 a name. */
static uint8_t word(void)
{
    wstart = src;
    const uint8_t *p = src;
    uint16_t t = lexr_keyword(&p);
    src = (uint8_t *)p;
    if (!t) {
        name();
        return 0;
    }
    if (t >= 0x100)
        *dst++ = TK_X;
    *dst++ = (uint8_t)t;
    return (uint8_t)t;
}

/* The line's end: its column, its 0. */
static uint8_t *finish(void)
{
    markcol();
    *dst++ = 0;
    return dst;
}

uint8_t *lexr_line(uint8_t *text, uint8_t *body)
{
    src = text;
    dst = body;
    lexr.src = text;
    lexr.dst = body;
    lexr.end = body + LMAX;
    lexr.last = -1;
    for (;;) {
        uint8_t c = *src++;
        if (c == 0) { /* the window's end: more, else the line's */
            src--;
            if (more())
                continue;
            src++;
            return finish();
        }
        if (c == ' ' || c == 9)
            continue;
        ahead();
        markcol();
        if (c == '"') { /* a string: to its quote, as it is */
            *dst++ = c;
            for (;;) {
                room();
                c = *src++;
                if (c == 0) {
                    src--;
                    if (more())
                        continue;
                    src++;
                    return finish();
                }
                *dst++ = c;
                if (c == '"')
                    break;
            }
            continue;
        }
        if (c == '\'') {
            *dst++ = TK_REM;
            *dst++ = REM_Q;
            goto rem;
        }
        if (c == '&') {
            hexnum();
            continue;
        }
        if (c == '.' && !isdigit_(*src)) { /* a '.' alone */
            *dst++ = c;
            continue;
        }
        if (c == '.' || isdigit_(c)) {
            src--;
            number();
            continue;
        }
        if (isletter(c)) {
            src--;
            joinfn();
            uint8_t t = word();
            if (t == TK_REM)
                goto rem;
            if (t != TK_DATA)
                continue;
            bool quoted = false; /* DATA's text: to a ':' out of quotes */
            for (;;) {
                room();
                c = *src++;
                if (c == 0) {
                    src--;
                    if (more())
                        continue;
                    src++;
                    return finish();
                }
                if (c == '"')
                    quoted = !quoted;
                *dst++ = c;
                if (!quoted && c == ':')
                    break;
            }
            continue;
        }
        uint8_t t = lexr_char_token[c];
        *dst++ = t ? t : c;
    }
rem: /* REM's text: the rest, as it is */
    for (;;) {
        room();
        uint8_t c = *src++;
        if (c == 0) {
            src--;
            if (more())
                continue;
            src++;
            *dst++ = 0;
            return dst;
        }
        *dst++ = c;
    }
}
