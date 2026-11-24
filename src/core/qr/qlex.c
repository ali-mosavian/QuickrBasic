/* Quickr's lexer: the source's lines to tokens.
 *
 * # How it works
 * A line is scanned left to right: blanks skipped, ' starts a comment to the line's end. A name
 * is a letter or _ then letters, digits and _; one spelled as a keyword, case and all, is that
 * keyword. Brackets ( [ { count a depth: a line's end is a statement's end (QK_NL) only at depth
 * 0, so an open bracket continues the statement on the next line.
 *
 * A number is 0x hex, 0b binary or decimal, _ allowed between digits; a point followed by a digit
 * or an exponent makes it a real. A string's escapes are \n \t \" and \\; its token keeps the
 * text as written, which codegen loads from the line itself.
 *
 * An f"..." is QK_FSTR, then its parts in order: QK_FPART text, or a value's tokens between
 * QK_LBRACE and QK_RBRACE, a QK_FSPEC after its top-level :; then QK_FEND. {{ and }} are one
 * brace of text.
 *
 * A line ending in a lambda's header, fn(x as T) [as U], starts a block: its lines are statements,
 * each ended, until the end fn that closes it gives the brackets back their depth.
 *
 * # Example
 *    let x = (1 +       let x = ( 1 +       no QK_NL: depth 1
 *             0x10)     0x10 ) QK_NL
 */
#include "qr/qlex.h"
#include "plat/pfpu.h"

enum { MANT_DIGITS = 19 }; /* a real's digits kept, past them dropped */

typedef struct {
    QSrc *src;
    QMem *mem;
    QErrs *errs;
    uint32_t cap;  /* src->toks' room */
    uint32_t line; /* the line scanned */
    const char *text;
    uint32_t len;
    uint32_t at;    /* the next byte */
    uint32_t depth; /* the brackets open */
    uint32_t *lams; /* each block lambda open: the depth its header ended at */
    uint32_t nlams, caplams;
} QLex;

static const char *const qlex_words[] = {
    [QK_AND - QK_AND] = "and",       [QK_AS - QK_AND] = "as",
    [QK_BREAK - QK_AND] = "break",   [QK_CASE - QK_AND] = "case",
    [QK_CONST - QK_AND] = "const",   [QK_CONTINUE - QK_AND] = "continue",
    [QK_DIV - QK_AND] = "div",       [QK_DO - QK_AND] = "do",
    [QK_ELSE - QK_AND] = "else",     [QK_ELSEIF - QK_AND] = "elseif",
    [QK_END - QK_AND] = "end",       [QK_ENUM - QK_AND] = "enum",
    [QK_FALSE - QK_AND] = "false",   [QK_FN - QK_AND] = "fn",
    [QK_FOR - QK_AND] = "for",       [QK_IF - QK_AND] = "if",
    [QK_IN - QK_AND] = "in",         [QK_IS - QK_AND] = "is",
    [QK_LET - QK_AND] = "let",       [QK_MATCH - QK_AND] = "match",
    [QK_MOD - QK_AND] = "mod",       [QK_MUT - QK_AND] = "mut",
    [QK_NONE - QK_AND] = "none",     [QK_NOT - QK_AND] = "not",
    [QK_OR - QK_AND] = "or",         [QK_RETURN - QK_AND] = "return",
    [QK_STEP - QK_AND] = "step",     [QK_THEN - QK_AND] = "then",
    [QK_TRUE - QK_AND] = "true",     [QK_TYPE - QK_AND] = "type",
    [QK_VAR - QK_AND] = "var",       [QK_WHILE - QK_AND] = "while",
    [QK_XOR - QK_AND] = "xor",       [QK_YIELD - QK_AND] = "yield",
    [QK_YIELDS - QK_AND] = "yields", [QK_FROM - QK_AND] = "from",
    [QK_IMPORT - QK_AND] = "import", [QK_PUBLIC - QK_AND] = "public",
    [QK_NATIVE - QK_AND] = "native", [QK_OPERATOR - QK_AND] = "operator",
    [QK_TEST - QK_AND] = "test",     [QK_ASSERT - QK_AND] = "assert",
    [QK_FUNCTION - QK_AND] = "function",
};

/* Punctuation, longest first where one starts another. */
static const struct {
    char text[3];
    QKind kind;
} qlex_puncts[] = {
    {"**", QK_POW},   {"..", QK_DOTDOT}, {"<>", QK_NE},    {"<=", QK_LE},    {"<<", QK_SHL},
    {">=", QK_GE},    {">>", QK_SHR},    {"+=", QK_ADDEQ}, {"-=", QK_SUBEQ}, {"*=", QK_MULEQ},
    {"/=", QK_DIVEQ}, {"??", QK_QQ},     {"(", QK_LPAREN}, {")", QK_RPAREN}, {"[", QK_LBRACK},
    {"]", QK_RBRACK}, {"{", QK_LBRACE},  {"}", QK_RBRACE}, {",", QK_COMMA},  {":", QK_COLON},
    {".", QK_DOT},    {"?", QK_QUEST},   {"!", QK_BANG},   {"+", QK_PLUS},   {"-", QK_MINUS},
    {"*", QK_STAR},   {"/", QK_SLASH},   {"=", QK_EQ},     {"<", QK_LT},     {">", QK_GT},
    {"&", QK_AMP},    {"|", QK_BAR},     {"^", QK_CARET},  {"~", QK_TILDE},
};

static bool qlex_word_char(const uint8_t c) { return is_word_char(c); }

/* A new token of kind from byte from to the cursor. */
static QTok *qlex_tok(QLex *const L, const QKind kind, const uint32_t from)
{
    QSrc *const s = L->src;
    s->toks = qast_room(L->mem, s->toks, s->ntoks, &L->cap, sizeof *s->toks);
    QTok *const t = &s->toks[s->ntoks++];
    *t = (QTok){
        .kind = kind,
        .line = L->line,
        .col = from,
        .len = L->at - from,
        .text = L->text + from,
    };

    return t;
}

/* An error at the token t, which becomes QK_BAD. */
static void qlex_bad(QLex *const L, QTok *const t, const char *const msg)
{
    qast_error(L->errs, t, msg, t->text, t->len);
    t->kind = QK_BAD;
}

static uint8_t qlex_peek(const QLex *const L, const uint32_t ahead)
{
    return L->at + ahead < L->len ? (uint8_t)L->text[L->at + ahead] : 0;
}

/* A name, or the keyword it spells. */
static void qlex_name(QLex *const L, const uint32_t from)
{
    while (qlex_word_char(qlex_peek(L, 0)))
        L->at++;

    const uint32_t n = L->at - from;
    const char *const w = L->text + from;
    QKind kind = QK_NAME;

    for (uint32_t k = 0; k < countof(qlex_words); k++)
        if (strlen(qlex_words[k]) == n && !memcmp(qlex_words[k], w, n))
            kind = (QKind)(QK_AND + k);

    qlex_tok(L, kind, from);
}

/* c's value as a digit of base, or base when it is none. */
static uint32_t qlex_digit(const uint8_t c, const uint32_t base)
{
    const uint32_t d = is_digit(c)    ? (uint32_t)(c - '0')
                       : is_letter(c) ? (uint32_t)(upcase(c) - 'A' + 10)
                                      : base;
    return d < base ? d : base;
}

/* Digits of base at the cursor, _ between them; their value to *v, false past 2^63 - 1 (*v kept
   at the last that fit), *bad on a misplaced _. Returns the digits read. */
static uint32_t qlex_digits(
    QLex *const L,
    const uint32_t base,
    uint64_t *const v,
    bool *const bad,
    bool *const over
)
{
    uint32_t n = 0;

    for (;;) {
        const uint8_t c = qlex_peek(L, 0);

        if (c == '_') {
            if (!n || qlex_digit(qlex_peek(L, 1), base) == base)
                *bad = true;

            L->at++;
            continue;
        }

        const uint32_t d = qlex_digit(c, base);

        if (d == base)
            return n;

        if (*v > UINT64_MAX / base || *v * base > UINT64_MAX - d)
            *over = true;
        else
            *v = *v * base + d;

        L->at++;
        n++;
    }
}

/* A decimal real's value: its digits at from (_ skipped) to the cursor, a point and an exponent. */
static double qlex_real(const QLex *const L, const uint32_t from, bool *const over)
{
    uint64_t mant = 0;
    int32_t pow10 = 0, exp = 0, esign = 1;
    uint32_t kept = 0;
    bool point = false, in_exp = false;

    for (uint32_t i = from; i < L->at; i++) {
        const uint8_t c = (uint8_t)L->text[i];

        if (c == '_')
            continue;
        if (c == '.') {
            point = true;
            continue;
        }

        if (c == 'e' || c == 'E') {
            in_exp = true;
            continue;
        }

        if (in_exp && (c == '+' || c == '-')) {
            esign = c == '-' ? -1 : 1;
            continue;
        }

        if (in_exp) {
            if (exp < 100000)
                exp = exp * 10 + (c - '0');

            continue;
        }

        if (!mant && c == '0') { /* a leading zero */
            pow10 -= point;
            continue;
        }

        if (kept < MANT_DIGITS) {
            mant = mant * 10 + (uint64_t)(c - '0');
            kept++;
            pow10 -= point;
        } else
            pow10 += !point;
    }

    double d = 0;

    if (mant)
        pfpu_scale10(mant, pow10 + esign * exp, &d);

    const uint64_t bits = (Cell){.d = d}.raw;

    *over = (bits >> 52 & 0x7FF) == 0x7FF;
    return d;
}

/* A number: 0x.., 0b.., or decimal, maybe a real. */
static void qlex_number(QLex *const L, const uint32_t from)
{
    uint32_t base = 10;
    const uint8_t x = qlex_peek(L, 1);

    if (qlex_peek(L, 0) == '0' && (x == 'x' || x == 'X' || x == 'b' || x == 'B')) {
        base = x == 'x' || x == 'X' ? 16 : 2;
        L->at += 2;
    }

    uint64_t v = 0;
    bool bad = false, over = false;
    const uint32_t n = qlex_digits(L, base, &v, &bad, &over);
    QKind kind = QK_INT;

    if (base == 10 && qlex_peek(L, 0) == '.' && is_digit(qlex_peek(L, 1))) {
        L->at++;
        uint64_t frac = 0;
        bool fover = false;
        qlex_digits(L, 10, &frac, &bad, &fover);
        kind = QK_REAL;
    }

    const uint8_t e = qlex_peek(L, 0), sign = qlex_peek(L, 1);

    if (base == 10 && (e == 'e' || e == 'E') &&
        (is_digit(sign) || ((sign == '+' || sign == '-') && is_digit(qlex_peek(L, 2))))) {
        L->at += 1 + !is_digit(sign);
        uint64_t ev = 0;
        bool eover = false;
        qlex_digits(L, 10, &ev, &bad, &eover);
        kind = QK_REAL;
    }

    while (qlex_word_char(qlex_peek(L, 0))) { /* 12ab, 0x: one bad token */
        bad = true;
        L->at++;
    }

    QTok *const t = qlex_tok(L, kind, from);

    if (bad || !n) {
        qlex_bad(L, t, "bad number {}");
        return;
    }

    if (kind == QK_REAL)
        t->d = qlex_real(L, from, &over);
    else {
        t->i = (int64_t)v;
        t->big = v > INT64_MAX;
    }

    if (over)
        qlex_bad(L, t, "number {} too large");
}

/* An escape at the cursor, \ then its letter, checked; L->at past it. */
static void qlex_escape(QLex *const L, bool *const ok, uint32_t *const bad_at)
{
    const uint8_t e = qlex_peek(L, 1);

    if (e != 'n' && e != 't' && e != '"' && e != '\\' && *ok) {
        *ok = false;
        *bad_at = L->at;
    }

    L->at += 1 + (L->at + 1 < L->len);
}

/* A string: its text between its quotes, its escapes checked. */
static void qlex_string(QLex *const L, const uint32_t from)
{
    L->at++;
    bool closed = false, escape_ok = true;
    uint32_t bad_at = 0;

    while (L->at < L->len) {
        const uint8_t c = (uint8_t)L->text[L->at];

        if (c == '"') {
            closed = true;
            break;
        }

        if (c == '\\') {
            qlex_escape(L, &escape_ok, &bad_at);
            continue;
        }

        L->at++;
    }

    L->at += closed;
    QTok *const t = qlex_tok(L, QK_STR, from);

    if (!closed) {
        qlex_bad(L, t, "string not closed");
        return;
    }

    if (!escape_ok) {
        const QTok at = {.line = L->line, .col = bad_at};

        qast_error(L->errs, &at, "unknown escape {}", L->text + bad_at, 2);
        t->kind = QK_BAD;
        return;
    }

    t->text++; /* between the quotes */
    t->len -= 2;
}

/* Punctuation at the cursor; a byte that is none is an error. */
static void qlex_punct(QLex *const L, const uint32_t from)
{
    for (uint32_t k = 0; k < countof(qlex_puncts); k++) {
        const uint32_t n = qlex_puncts[k].text[1] ? 2 : 1;

        if (L->at + n > L->len || memcmp(qlex_puncts[k].text, L->text + L->at, n))
            continue;

        const QKind kind = qlex_puncts[k].kind;
        L->at += n;
        qlex_tok(L, kind, from);

        if (kind == QK_LPAREN || kind == QK_LBRACK || kind == QK_LBRACE)
            L->depth++;
        if ((kind == QK_RPAREN || kind == QK_RBRACK || kind == QK_RBRACE) && L->depth)
            L->depth--;

        return;
    }

    L->at++;
    qlex_bad(L, qlex_tok(L, QK_BAD, from), "unexpected character {}");
}

static void qlex_fstring(QLex *L, uint32_t from);

/* The token at the cursor, a blank's none: false at a comment. */
static bool qlex_one(QLex *const L)
{
    const uint8_t c = (uint8_t)L->text[L->at];
    const uint32_t from = L->at;

    if (is_blank(c)) {
        L->at++;
        return true;
    }

    if (c == '\'')
        return false;

    if (c == 'f' && qlex_peek(L, 1) == '"')
        qlex_fstring(L, from);
    else if (is_letter(c) || c == '_')
        qlex_name(L, from);
    else if (is_digit(c))
        qlex_number(L, from);
    else if (c == '"')
        qlex_string(L, from);
    else
        qlex_punct(L, from);

    return true;
}

/* An f-string's text up to a value, a brace or its end: a QK_FPART; {{ and }} one brace. */
static void qlex_fpart(QLex *const L)
{
    const uint32_t from = L->at;
    bool ok = true;
    uint32_t bad_at = 0;

    while (L->at < L->len) {
        const uint8_t c = (uint8_t)L->text[L->at];

        if (c == '\\') {
            qlex_escape(L, &ok, &bad_at);
            continue;
        }

        if (c == '"')
            break;
        if ((c == '{' || c == '}') &&
            qlex_peek(L, 1) == c) { /* the first brace kept, the second skipped */
            L->at++;
            qlex_tok(L, QK_FPART, from);
            L->at++;

            if (!ok)
                break;

            qlex_fpart(L);
            return;
        }

        if (c == '{' || c == '}')
            break;

        L->at++;
    }

    if (L->at > from)
        qlex_tok(L, QK_FPART, from);

    if (!ok) {
        const QTok at = {.line = L->line, .col = bad_at};

        qast_error(L->errs, &at, "unknown escape {}", L->text + bad_at, 2);
    }
}

/* An f-string's {value[:spec]}: QK_LBRACE, the value's tokens, QK_FSPEC, QK_RBRACE. False: the
   line ended first. */
static bool qlex_fvalue(QLex *const L)
{
    L->at++;
    qlex_tok(L, QK_LBRACE, L->at - 1);
    const uint32_t depth = L->depth;

    while (L->at < L->len) {
        const uint8_t c = (uint8_t)L->text[L->at];

        if (L->depth == depth && (c == '}' || c == ':')) {
            if (c == ':') {
                const uint32_t from = ++L->at;

                while (L->at < L->len && L->text[L->at] != '}')
                    L->at++;

                qlex_tok(L, QK_FSPEC, from);
                continue;
            }

            L->at++;
            qlex_tok(L, QK_RBRACE, L->at - 1);
            return true;
        }

        if (!qlex_one(L))
            break;
    }

    return false;
}

/* f"..": QK_FSTR, its parts and values, QK_FEND. */
static void qlex_fstring(QLex *const L, const uint32_t from)
{
    L->at += 2;
    QTok *const start = qlex_tok(L, QK_FSTR, from);

    for (;;) {
        qlex_fpart(L);

        if (L->at >= L->len)
            break;

        const uint8_t c = (uint8_t)L->text[L->at];

        if (c == '"') {
            L->at++;
            qlex_tok(L, QK_FEND, L->at - 1);
            return;
        }

        if (c == '}') {
            L->at++;
            qlex_bad(L, qlex_tok(L, QK_BAD, L->at - 1), "a } in an f-string is written }}");
            continue;
        }

        if (!qlex_fvalue(L))
            break;
    }

    qlex_bad(L, start, "string not closed");
}

/* Can token kind k be part of a type after a lambda header's as, at bracket depth d past it? */
static bool qlex_typetok(const QKind k, const int32_t d)
{
    return k == QK_NAME || k == QK_LBRACK || k == QK_RBRACK || k == QK_INT || k == QK_QUEST ||
           k == QK_BANG || k == QK_LPAREN || k == QK_RPAREN || k == QK_FUNCTION || k == QK_AS ||
           (k == QK_COMMA && d > 0);
}

/* Do the tokens from first to the last end in a lambda's header: fn(..) [as T], its fn not a
   type's (after as)? */
static bool qlex_header(const QSrc *const s, const uint32_t first)
{
    uint32_t f = s->ntoks;

    for (uint32_t i = s->ntoks; i-- > first;)
        if (s->toks[i].kind == QK_FN && i + 1 < s->ntoks && s->toks[i + 1].kind == QK_LPAREN &&
            (!i || s->toks[i - 1].kind != QK_AS)) {
            f = i;
            break;
        }

    if (f == s->ntoks)
        return false;

    int32_t d = 0;
    uint32_t i = f + 1;

    for (; i < s->ntoks; i++) {
        d += s->toks[i].kind == QK_LPAREN;
        d -= s->toks[i].kind == QK_RPAREN;

        if (!d)
            break;
    }

    if (i == s->ntoks)
        return false;
    if (++i == s->ntoks)
        return true;
    if (s->toks[i].kind != QK_AS)
        return false;

    for (d = 0; i < s->ntoks; i++) {
        const QKind k = s->toks[i].kind;
        d += k == QK_LPAREN || k == QK_LBRACK;
        d -= k == QK_RPAREN || k == QK_RBRACK;

        if (d < 0 || !qlex_typetok(k, d))
            return false;
    }

    return !d;
}

/* Does the line start with end fn? */
static bool qlex_endfn(const QLex *const L)
{
    uint32_t i = 0;

    while (i < L->len && is_blank((uint8_t)L->text[i]))
        i++;

    if (L->len - i < 3 || memcmp(L->text + i, "end", 3))
        return false;

    i += 3;
    const uint32_t gap = i;

    while (i < L->len && is_blank((uint8_t)L->text[i]))
        i++;

    return i > gap && L->len - i >= 2 && !memcmp(L->text + i, "fn", 2) &&
           (L->len - i == 2 || !qlex_word_char((uint8_t)L->text[i + 2]));
}

/* Line i's tokens; QK_NL after them at depth 0, and after a block lambda's header. */
static void qlex_line(QLex *const L, const uint32_t i)
{
    const QLine *const ln = &L->src->lines[i];
    L->line = i;
    L->text = ln->text;
    L->len = ln->len;
    L->at = 0;

    if (L->nlams && !L->depth && qlex_endfn(L))
        L->depth = L->lams[--L->nlams];

    const uint32_t first = L->src->ntoks;

    while (L->at < L->len && qlex_one(L))
        ;

    const QSrc *const s = L->src;
    const bool open = s->ntoks && s->toks[s->ntoks - 1].kind != QK_NL;

    if (L->depth && open && qlex_header(s, first)) {
        L->lams = qast_room(L->mem, L->lams, L->nlams, &L->caplams, sizeof *L->lams);
        L->lams[L->nlams++] = L->depth;
        L->depth = 0;
    }

    if (!L->depth && open) {
        L->at = L->len;
        qlex_tok(L, QK_NL, L->len);
    }
}

void qlex_source(QSrc *const src, const uint32_t first, QMem *const m, QErrs *const e)
{
    QLex L = {
        .src = src,
        .mem = m,
        .errs = e,
        .line = first,
    };

    src->toks = nullptr;
    src->ntoks = 0;

    for (uint32_t i = first; i < src->nlines; i++)
        qlex_line(&L, i);

    if (src->ntoks && src->toks[src->ntoks - 1].kind != QK_NL) /* a bracket left open */
        qlex_tok(&L, QK_NL, L.at);

    L.at = L.len;
    qlex_tok(&L, QK_EOF, L.at);
}

const char *qlex_keyword(const QKind k)
{
    return k >= QK_AND && k <= QK_FUNCTION ? qlex_words[k - QK_AND] : nullptr;
}
