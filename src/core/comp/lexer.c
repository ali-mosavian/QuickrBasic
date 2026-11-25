/* The tokenizer: a typed line becomes the body of a stored line.
 *
 * # How it works
 * In the body:
 *   M_VAR len name nam:4  a name as typed, its suffix too; nam its entry in the name table
 *   M_INT n:4             a whole number under 2^31, &H, &O
 *   a token               80h..FEh, or TK_X and a byte (comp/dfa.h)
 * Spaces go, but in strings, REM and DATA. A number with a point, an exponent or 2^31 and over
 * stays text. A keyword is a whole word (the scanner's automaton, kwstep), any other word a name.
 * FN is joined to its name; E and D of an exponent go upper case.
 *
 * Lexr.off: the words a module's OPTION DISABLE made names (io/repl.c sets it), QB's statements
 * and functions for devices (the screen, files, sound, ports, the clock), the standard library's
 * to give: `line`, `open`, `print`. PRINT's off, ? is no PRINT. The language's own words stay.
 *
 * Lexr.col: an item's column in the text, by its body offset (the editor's). A streamed text is
 * read through a window: its LexrSource refills it. Names come from a LexrNames. This file also
 * defines dfa.h's tables (DFA_DATA).
 *
 * # Example
 *    x = 5 + y     M_VAR 1 "x" nam, TK_EQ, M_INT 5, TK_PLUS, M_VAR 1 "y" nam
 *
 * # asm structure (retired/asm32/comp/lex.asm)
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
 * | lx.dproc, lx.ntyps, lx.dtype | decl.proc, decl.ntypes, decl.type (declare.c) | MOVED |
 * | lx.nflds | | D4 |
 * | lx.lxf, lx.lxn, lx.lxs | locals | MIRROR |
 * | lx.nlen, lx.nptr | symb_find_name's parameters | D1 |
 * | lx.lxst, lx.nkey, lx.nlenp, lx.lxv, lx.lxm | | D2 |
 *
 * - D1. The name's length and pointer are parameters, not globals.
 * - D2. Unused in the asm; dropped.
 * - D3. It takes its own state; names, streamed text and errors come through LexrNames,
 *   LexrSource and Fault, which the session provides.
 * - D4. It counted the fields for a limit there no longer is; dropped.
 */
#define DFA_DATA
#include "comp/lexer.h"
#include "util/grow.h"
#include "comp/dfa.h"
#include "num/single.h"

enum { LXAHEAD = 128 }; /* ahead: a word's, a number's bytes in the window */

static void kw_made(void);

[[noreturn]]
static void lexr_fail(const Lexr *const lx)
{
    fault_error(lx->fault, E_OM);
}

void lexr_init(Lexr *const lx, Fault *const f, LexrNames *const names)
{
    lx->fault = f;
    lx->names = names;
    kw_made();
}

static void lexr_room(Lexr *const lx)
{
    if (lx->out >= lx->end)
        lexr_fail(lx);
}

/* lexr.near: LXAHEAD before the window's end while the source may have more, else never met. */
static void lexr_near(Lexr *const lx)
{
    lx->near = lx->source && !lx->drained ? lx->source->end - LXAHEAD : (const uint8_t *)UINTPTR_MAX;
}

/* Streamed text: the window's end at src; the window slid, its bytes from src on kept, and read
   on. False: the line has no more. */
static bool lexr_more(Lexr *const lx)
{
    if (!lx->source || lx->drained)
        return false;

    const uint8_t *keep = lx->in;

    if (!lx->source->more(lx->source, &keep)) {
        lx->drained = true; /* asked again before each item: a call each */
        lexr_near(lx);
        return false;
    }

    const ptrdiff_t moved = lx->in - keep;
    lx->src -= moved;
    lx->word -= moved;
    lx->in = (uint8_t *)keep;
    lexr_near(lx);
    return true;
}

/* Streamed: the item at src-1 and LXAHEAD bytes after it in the window. */
static void lexr_ahead(Lexr *const lx)
{
    if (lx->source && !lx->drained && lx->source->end - lx->in < LXAHEAD) {
        lx->in--;
        lexr_more(lx);
        lx->in++;
    }
}

/* The item at src-1: its column to lexr.col at the body's offset. */
static void lexr_markcol_kept(Lexr *const lx)
{
    const uint32_t off = (uint32_t)(lx->out - lx->dst);

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

static inline void lexr_markcol(Lexr *const lx)
{
    if ((uint32_t)(lx->out - lx->dst) >= LMAX)
        lexr_fail(lx);
    if (lx->cols)
        lexr_markcol_kept(lx);
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
/* A whole number of up to 9 digits, nothing after it that makes it text: M_INT n:4, true. False,
   nothing read: lexr_number's. */
static bool lexr_int(Lexr *const lx)
{
    const uint8_t *p = lx->in;
    uint32_t v = 0;

    for (; is_digit(*p) && p - lx->in < 9; p++)
        v = v * 10 + (uint32_t)(*p - '0');

    const uint8_t u = upcase(*p);

    if (is_digit(*p) || *p == '.' || *p == '#' || *p == '!' || u == 'E' || u == 'D' ||
        (!*p && lx->source && !lx->drained) || lx->end - lx->out < 5)
        return false;

    lx->in = (uint8_t *)p + (*p == '%' || *p == '&'); /* an INTEGER's, a LONG's: one type */
    *lx->out++ = M_INT;
    store32(lx->out, v);
    lx->out += 4;
    return true;
}

static void lexr_number(Lexr *const lx)
{
    if (lexr_int(lx))
        return;

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

enum {
    KW_STATES = countof(lexr_kw_token),
    KW_ROW = 32, /* a row's classes, a power of two: a row's index a shift */
    KW_NOT = 0xFF,
};
static_assert(countof(lexr_kw_start) <= KW_ROW);

/* The scanner's transitions, a row a state (0 none), from its lists the first keyword scanned: a
   step a table read, not a search of the state's list (7% of a large program's load). kw_class:
   each byte's class in a word, any case (a digit's, '.''s or '_''s 0, no keyword's), KW_NOT none. */
static uint16_t kw_next[KW_STATES][KW_ROW];
static uint8_t kw_class[256];

static void kw_dense(void)
{
    for (uint32_t st = 0; st < KW_STATES; st++)
        for (uint32_t i = lexr_kw_first[st]; i < lexr_kw_first[st + 1]; i++)
            kw_next[st][lexr_kw_char[i]] = lexr_kw_next[i];

    for (uint32_t c = 0; c < 256; c++)
        kw_class[c] = is_name_char((uint8_t)c)
                          ? lexr_kw_class[upcase((uint8_t)c)]
                          : KW_NOT;
}

/* QB's words for devices: OPTION DISABLE's. */
static const uint16_t device_words[] = {
    TK_DATA,   TK_INPUT,  TK_READ,     TK_RUN,    TK_RES,      TK_WAIT,   TK_POKE,     TK_PRINT,
    TK_CLR,    TK_OPEN,   TK_CLOSE,    TK_GET,    TK_TAB,      TK_SPC,    TK_FRE,      TK_POS,
    TK_PEEK,   TK_CLS,    TK_LOCATE,   TK_COLOR,  TK_SYSTEM,   TK_INKEYS,
#define X(t) 0x100 + TK1_##t
    X(ACCESS), X(APPEND), X(BEEP),     X(BINARY), X(BLOAD),    X(BSAVE),  X(CHAIN),    X(CHDIR),
    X(CIRCLE), X(COM),    X(COMMANDS), X(CSRLIN), X(DATES),    X(DRAW),   X(ENVIRON),  X(ENVIRONS),
    X(EOF),    X(ERDEV),  X(ERDEVS),   X(FIELD),  X(FILEATTR), X(FILES),  X(FREEFILE), X(INP),
    X(INPUTS), X(IOCTL),  X(IOCTLS),   X(KEY),    X(KILL),     X(LINE),   X(LOC),      X(LOCK),
    X(LOF),    X(LPOS),   X(LPRINT),   X(LSET),   X(MKDIR),    X(NAME),   X(OFF),      X(OUT),
    X(OUTPUT), X(PAINT),  X(PALETTE),  X(PCOPY),  X(PEN),      X(PLAY),   X(PMAP),     X(POINT),
    X(PRESET), X(PSET),   X(PUT),      X(RANDOM), X(RESET),    X(RMDIR),  X(RSET),     X(SCREEN),
    X(SEEK),   X(SETMEM), X(SHELL),    X(SIGNAL), X(SLEEP),    X(SOUND),  X(STICK),    X(STRIG),
    X(TIMER),  X(TIMES),  X(TROFF),    X(TRON),   X(UEVENT),   X(UNLOCK), X(USING),    X(VIEW),
    X(WIDTH),  X(WINDOW), X(WRITE),
#undef X
};

static bool lexr_device(const uint16_t t)
{
    for (uint32_t i = 0; i < countof(device_words); i++)
        if (device_words[i] == t)
            return true;

    return false;
}

static bool dense;

/* lexr_keyword's, the tables made */
static inline uint16_t kw_scan(const uint8_t **const at)
{
    const uint8_t *p = *at;
    uint16_t st = KW_START;
    uint8_t k = kw_class[*p];

    if (k != KW_NOT) { /* the start's step: its row is whole */
        st = lexr_kw_start[k];

        while ((k = kw_class[*++p]) != KW_NOT)
            st = kw_next[st][k];
    }

    if (*p == '$') { /* a '$' when the keyword has one */
        const uint16_t s2 = kw_next[st][lexr_kw_class['$']];

        if (lexr_kw_token[s2]) {
            st = s2;
            p++;
        }
    }

    *at = p;
    return lexr_kw_token[st];
}

static void kw_made(void)
{
    if (!dense) {
        kw_dense();
        dense = true;
    }
}

uint16_t lexr_keyword(const uint8_t **const at)
{
    kw_made();
    return kw_scan(at);
}

/* The keyword at *at in a body, its token or a name that spells it (a word disabled before): its
   token (100h + n for TK_X n), *at past it; 0 neither. */
static uint16_t body_keyword(const uint8_t **const at)
{
    const uint8_t *p = *at;
    uint16_t t = 0;

    if (*p == TK_X) {
        t = 0x100 + p[1];
        p += 2;
    } else if (*p >= 0x80) {
        t = *p++;
    } else if (*p == M_VAR) {
        uint8_t text[256];
        const uint8_t *q = text;
        memcpy(text, p + 2, p[1]);
        text[p[1]] = 0;
        t = kw_scan(&q);
        t = *q ? 0 : t;
        p += 2 + p[1] + NAME_ID;
    }

    *at = p;
    return t;
}

Disable lexr_disable(const uint8_t *p, uint64_t off[8], const uint8_t **const bad)
{
    static const char disable[] = "DISABLE";

    if (*p++ != TK_OPTION || *p != M_VAR || p[1] != sizeof disable - 1)
        return DISABLE_NONE;

    for (uint32_t i = 0; disable[i]; i++)
        if (upcase(p[2 + i]) != (uint8_t)disable[i])
            return DISABLE_NONE;

    kw_made();
    p += 2 + p[1] + NAME_ID;
    uint64_t words[8];
    memcpy(words, off, sizeof words);

    for (;;) {
        *bad = p;
        const uint16_t t = body_keyword(&p);

        if (!t || !lexr_device(t))
            return DISABLE_BAD;

        words[t >> 6] |= 1ull << (t & 63);

        if (*p == ',') {
            p++;
            continue;
        }

        if (*p && *p != ':' && *p != TK_REM) {
            *bad = p;
            return DISABLE_BAD;
        }

        break;
    }

    memcpy(off, words, sizeof words);
    return DISABLE_SET;
}

/* The word from wstart to src is a name: M_VAR len name nam:4, a suffix in the text but not the
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
    store32(lx->out, lx->names->intern(lx->names, text, n));
    lx->out += NAME_ID;
}

enum { LOOP_WORD = 5 }; /* WHILE: the longest of FOR, DO, WHILE */

/* What follows the word of n characters at lexr.in and its blanks: LOOP_WORD bytes of it in the
   window when the line has them. */
static const uint8_t *lexr_after(Lexr *const lx, const uint32_t n)
{
    for (;;) {
        const uint8_t *q = lx->in + n;

        while (is_blank(*q))
            q++;

        const bool whole = !lx->source || lx->drained || lx->source->end - q > LOOP_WORD;

        if (whole || !lexr_more(lx)) /* more: the window slid, lexr.in with it */
            return q;
    }
}

/* The word of n characters at lexr.in is CONTINUE: FOR, DO or WHILE follows it. Any other
   CONTINUE is the name it was before the statement. */
static bool lexr_continues(Lexr *const lx, const uint32_t n)
{
    const uint8_t *q = lexr_after(lx, n);
    const uint16_t t = kw_scan(&q);
    return t == TK_FOR || t == TK_DO || t == TK_WHILE;
}

/* The word of n characters at lexr.in starts an IMPORT or FROM statement: a name follows it, or
   for FROM a '.'. Else it is the name it was before them. */
static bool lexr_imports(Lexr *const lx, const uint32_t n, const bool from)
{
    const uint8_t c = *lexr_after(lx, n);
    return is_letter(c) || (from && c == '.');
}

/* The word of n characters at lexr.in is PRIVATE: SUB, FUNCTION, CONST or TYPE follows it. */
static bool lexr_private(Lexr *const lx, const uint32_t n)
{
    const uint8_t *q = lexr_after(lx, n);
    const uint16_t t = kw_scan(&q);
    return t == TK_SUB || t == TK_FUNCTION || t == TK_CONST || t == TK_TYPE;
}

/* The word of n characters at lexr.in is DIM's PUBLIC: a name not AS follows it. */
static bool lexr_public(Lexr *const lx, const uint32_t n)
{
    const uint8_t *q = lexr_after(lx, n);
    return is_letter(*q) && kw_scan(&q) != TK_AS;
}

/* Token t of the word of n characters at lexr.in, start where a statement starts, after DIM: 0
   when the word is a keyword only in some places, or disabled, and here a name. */
static uint16_t lexr_placed(
    Lexr *const lx,
    const uint16_t t,
    const uint32_t n,
    const bool start,
    const bool dim
)
{
    if (lexr_off(lx, t))
        return 0;

    switch (t) {
    case 0x100 + TK1_CONTINUE:
        return start && lexr_continues(lx, n) ? t : 0;

    case 0x100 + TK1_FROM:
        lx->from = start && lexr_imports(lx, n, true);
        return lx->from ? t : 0;

    case 0x100 + TK1_IMPORT:
        return lx->from || (start && lexr_imports(lx, n, false)) ? t : 0;

    case 0x100 + TK1_PRIVATE:
        return start && lexr_private(lx, n) ? t : 0;

    case 0x100 + TK1_PUBLIC:
        return dim && lexr_public(lx, n) ? t : 0;

    default:
        return t;
    }
}

/* A word, start where a statement starts, dim after DIM: its keyword's token written, else a name;
   the token (100h + n for TK_X n), 0 a name. */
static uint16_t lexr_word(Lexr *const lx, const bool start, const bool dim)
{
    lx->word = lx->in;
    const uint8_t *p = lx->in;
    const uint16_t scanned = kw_scan(&p);
    const uint32_t n = (uint32_t)(p - lx->in);
    const uint16_t t = lexr_placed(lx, scanned, n, start, dim);
    lx->in += n;

    if (!t) {
        lexr_name(lx);
        return 0;
    }

    if (t >= 0x100) {
        *lx->out++ = TK_X;
        lx->escaped = true;
    }

    *lx->out++ = (uint8_t)t;
    return t;
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

/* The window's bytes to lexr.out up to stop or a 0, while there is room: the pointers in locals, as
   a store through lexr.out may be to lexr itself. */
static void lexr_copy_to(Lexr *const lx, const uint8_t stop)
{
    const uint8_t *in = lx->in;
    uint8_t *out = lx->out;
    uint8_t *const end = lx->end;

    for (uint8_t c; (c = *in) && c != stop && out < end;)
        *out++ = c, in++;

    lx->in = (uint8_t *)in;
    lx->out = out;
}

/* A string's rest, to its quote, as it is. false: the line ended. */
static bool lexr_copy_string(Lexr *const lx)
{
    for (;;) {
        lexr_copy_to(lx, '"');
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
        const uint8_t *in = lx->in; /* the window's bytes by locals: see lexr_copy_to */
        uint8_t *out = lx->out;
        uint8_t *const end = lx->end;

        for (uint8_t c; (c = *in) && out < end;) {
            in++;
            *out++ = c;

            if (c == '"')
                quoted = !quoted;
            else if (c == ':' && !quoted) {
                lx->in = (uint8_t *)in;
                lx->out = out;
                return true;
            }
        }

        lx->in = (uint8_t *)in;
        lx->out = out;
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
        lexr_copy_to(lx, 0);
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
    lx->drained = false;
    lx->escaped = false;
    lx->start = true;
    lx->from = lx->dim = false;
    lexr_near(lx);

    for (;;) {
        uint8_t *in = lx->in;
        uint8_t c;

        while (is_blank(c = *in))
            in++;

        lx->in = in + 1;

        if (!c) { /* the window's end: refilled, or the line's */
            lx->in = in;
            c = lexr_next_char(lx);

            if (!c)
                return lexr_finish(lx);
            if (is_blank(c))
                continue;
        }

        if (lx->in > lx->near)
            lexr_ahead(lx);
        if (lx->out >= lx->end) /* lexr_markcol's, end the body's LMAX */
            lexr_fail(lx);
        if (lx->cols)
            lexr_markcol_kept(lx);

        const bool start = lx->start, first = lx->out == lx->dst, dim = lx->dim;
        lx->start = lx->dim = false;

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
            lx->start = first; /* the line's number */
        } else if (is_letter(c)) {
            lx->in--;
            lexr_joinfn(lx);
            const uint16_t t = lexr_word(lx, start, dim);
            lx->start = t == TK_THEN || t == TK_ELSE;
            lx->dim = t == TK_DIM;

            if (t == TK_REM)
                return lexr_copy_rest(lx);
            if (t == TK_DATA && !lexr_copy_data(lx))
                return lexr_finish(lx);
        } else if (c == '(' && lx->in[0] == ':' && lx->in[1] == ')') { /* ours: an array whole */
            lx->in += 2;
            *lx->out++ = TK_X;
            *lx->out++ = TK1_WHOLE;
        } else {
            const uint8_t t = lexr_char_token[c];
            *lx->out++ = t && !lexr_off(lx, t) ? t : c; /* ? is PRINT's */
            lx->start = c == ':';
            lx->from &= !lx->start;
        }
    }
}
