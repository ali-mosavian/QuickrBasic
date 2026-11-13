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


static void room(void)
{
    if (lexr.out >= lexr.end)
        repl_error(E_OM);
}

/* Streamed text: the window's end at src; the window slid, its bytes from src on kept, and read
   on. False: the line has no more. */
static bool more(void)
{
    if (!lexr.more)
        return false;
    const uint8_t *keep = lexr.in;
    if (!lexr.more(&keep))
        return false;
    const ptrdiff_t moved = lexr.in - keep;
    lexr.src -= moved;
    lexr.word -= moved;
    lexr.in = (uint8_t *)keep;
    return true;
}

/* Streamed: the item at src-1 and LXAHEAD bytes after it in the window. */
static void ahead(void)
{
    if (lexr.more && srcr.end - lexr.in < LXAHEAD) {
        lexr.in--;
        more();
        lexr.in++;
    }
}

/* The item at src-1: its column to lexr.col at the body's offset. */
static void markcol(void)
{
    const uint32_t off = (uint32_t)(lexr.out - lexr.dst);
    if (off >= LMAX)
        repl_error(E_OM);
    if (off >= LXCMAX)
        return;
    if (lexr.all)
        for (int32_t i = lexr.last + 1; i < (int32_t)off; i++)
            lexr.col[i] = lexr.lastcol;
    const uint16_t col = (uint16_t)(lexr.in - 1 - lexr.src);
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
    if (upcase(lexr.in[0]) != 'F' || upcase(lexr.in[1]) != 'N' || !is_blank(lexr.in[2]))
        return;
    uint8_t *b = lexr.in + 2;
    while (is_blank(*b))
        b++;
    if (!is_letter(*b))
        return;
    const uint8_t f = lexr.in[0];
    const uint8_t n = lexr.in[1];
    lexr.in[0] = lexr.in[1] = ' '; /* blanks first: one may overlap */
    b[-2] = f;
    b[-1] = n;
    lexr.in = b - 2;
}

/* A number: M_INT n:4 when whole and under 2^31, else its text. */
static void number(void)
{
    uint8_t *const start = lexr.out;
    int64_t v = 0;
    bool text = false;
    for (;;) {
        room();
        if (*lexr.in == 0)
            more();
        const uint8_t c = *lexr.in;
        if (is_digit(c)) {
            *lexr.out++ = *lexr.in++;
            if (text)
                continue;
            v = v * 10 + (c - '0');
            if (v > INT32_MAX)
                text = true; /* 2^31 and over */
            continue;
        }
        if (c == '.') {
            *lexr.out++ = *lexr.in++;
            text = true;
            continue;
        }
        const uint8_t u = upcase(c);
        if (u == 'E' || u == 'D') { /* an exponent when a digit or sign follows */
            const uint8_t b = lexr.in[1];
            if (b == '+' || b == '-') {
                *lexr.in = u;
                *lexr.out++ = *lexr.in++;
                *lexr.out++ = *lexr.in++;
                text = true;
                continue;
            }
            if (is_digit(b)) {
                *lexr.in = u;
                *lexr.out++ = *lexr.in++;
                text = true;
                continue;
            }
        }
        break;
    }
    if (*lexr.in == '#' || *lexr.in == '!') { /* a DOUBLE's, a SINGLE's */
        *lexr.out++ = *lexr.in++;
        text = true;
    }
    if (text)
        return;
    if (*lexr.in == '%' || *lexr.in == '&') /* an INTEGER's, a LONG's: one type */
        lexr.in++;
    lexr.out = start;
    *lexr.out++ = M_INT;
    store32(lexr.out, (uint32_t)v);
    lexr.out += 4;
}

/* &H or &O, its digits, a % or & after: M_INT n:4; '&' alone is the byte. src after '&'. */
static void hexnum(void)
{
    uint32_t v;
    const uint8_t *p = lexr.in;
    if (!sngl_hexoct(&p, &v)) {
        *lexr.out++ = '&';
        return;
    }
    lexr.in = (uint8_t *)p;
    if (*lexr.in == '%' || *lexr.in == '&')
        lexr.in++;
    *lexr.out++ = M_INT;
    store32(lexr.out, v);
    lexr.out += 4;
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
static void name(void)
{
    *lexr.out++ = M_VAR;
    uint8_t *const len = lexr.out++;
    uint8_t *const text = lexr.out;
    const uint8_t n = (uint8_t)(lexr.in - lexr.word);
    memcpy(lexr.out, lexr.word, n);
    lexr.out += n;
    const uint8_t c = *lexr.in;
    if (c == '%' || c == '&' || c == '!' || c == '#' || c == '$')
        *lexr.out++ = *lexr.in++;
    *len = (uint8_t)(lexr.out - text);
    const uint16_t id = symb_find_name(text, n);
    store16(lexr.out, id);
    lexr.out += 2;
}

/* A word: its keyword's token written, else a name; the token, 0 a name. */
static uint8_t word(void)
{
    lexr.word = lexr.in;
    const uint8_t *p = lexr.in;
    const uint16_t t = lexr_keyword(&p);
    lexr.in = (uint8_t *)p;
    if (!t) {
        name();
        return 0;
    }
    if (t >= 0x100)
        *lexr.out++ = TK_X;
    *lexr.out++ = (uint8_t)t;
    return (uint8_t)t;
}

/* The line's end: its column, its 0. */
static uint8_t *finish(void)
{
    markcol();
    *lexr.out++ = 0;
    return lexr.out;
}

/* The next byte of the text, the window refilled first, src past it: 0 at the line's end, src
   past the 0 too (its column is the end's). */
static uint8_t next_char(void)
{
    while (!*lexr.in && more())
        ;
    return *lexr.in++;
}

/* A string's rest, to its quote, as it is. false: the line ended. */
static bool copy_string(void)
{
    for (;;) {
        room();
        const uint8_t c = next_char();
        if (!c)
            return false;
        *lexr.out++ = c;
        if (c == '"')
            return true;
    }
}

/* DATA's text: to a ':' out of quotes. false: the line ended. */
static bool copy_data(void)
{
    bool quoted = false;
    for (;;) {
        room();
        const uint8_t c = next_char();
        if (!c)
            return false;
        if (c == '"')
            quoted = !quoted;
        *lexr.out++ = c;
        if (!quoted && c == ':')
            return true;
    }
}

/* REM's text: the rest, as it is, then the 0. */
static uint8_t *copy_rest(void)
{
    for (;;) {
        room();
        const uint8_t c = next_char();
        *lexr.out++ = c;
        if (!c)
            return lexr.out;
    }
}


uint8_t *lexr_line(uint8_t *const text, uint8_t *const body)
{
    lexr.in = text;
    lexr.out = body;
    lexr.src = text;
    lexr.dst = body;
    lexr.end = body + LMAX;
    lexr.last = -1;
    for (;;) {
        const uint8_t c = next_char();
        if (!c)
            return finish();
        if (is_blank(c))
            continue;
        ahead();
        markcol();
        if (c == '"') {
            *lexr.out++ = c;
            if (!copy_string())
                return finish();
        } else if (c == '\'') {
            *lexr.out++ = TK_REM;
            *lexr.out++ = REM_Q;
            return copy_rest();
        } else if (c == '&')
            hexnum();
        else if (c == '.' && !is_digit(*lexr.in)) /* a '.' alone */
            *lexr.out++ = c;
        else if (c == '.' || is_digit(c)) {
            lexr.in--;
            number();
        } else if (is_letter(c)) {
            lexr.in--;
            joinfn();
            const uint8_t t = word();
            if (t == TK_REM)
                return copy_rest();
            if (t == TK_DATA && !copy_data())
                return finish();
        } else {
            const uint8_t t = lexr_char_token[c];
            *lexr.out++ = t ? t : c;
        }
    }
}
