/* The editor's auto indent: Enter puts the new line in by the blocks the line opens, and a line
 * starting with a block's end back at the block's start.
 *
 * # How it works
 * A line's statements are read by their first words as events: a block opened (FOR, DO, WHILE,
 * an IF ending in THEN, SELECT, SUB, FUNCTION, TYPE, a DEF FN with no '='), a middle (ELSE,
 * ELSEIF, CASE) or an end (NEXT, LOOP, WEND, END IF and the other ENDs). An IF ending other than
 * in THEN is a line's IF: it and the line's rest open nothing.
 *
 * A line starting with an end or a middle takes its block's start's indent, found up the lines,
 * the blocks of its kind between counted; a CASE one step in from its SELECT. The new line is a
 * step in for each block the line leaves open, a middle at its start opening its own.
 *
 * # Example
 *   FOR i = 1 TO 3       Enter: the next line 4 in
 *       PRINT i
 *       NEXT             Enter: NEXT back to FOR's column, the next line there too
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | indt_enter | NEW |
 */
#include "ui/indent.h"
#include "ui/ui.h"

typedef enum : uint8_t { K_FOR, K_DO, K_WHILE, K_IF, K_SEL, K_SUB, K_FUNC, K_TYPE, K_DEF } Kind;
typedef enum : int8_t { EV_END = -1, EV_MID = 0, EV_OPEN = 1 } What;

typedef struct {
    Kind kind;
    What what;
    bool lead; /* in the line's first statement */
} Ev;

enum { NEV = 32, WORD = 12 };

typedef struct {
    const uint8_t *p, *end;
} Cur;

static void indt_blanks(Cur *const c)
{
    while (c->p < c->end && *c->p == ' ')
        c->p++;
}

/* The next word in capitals to w, 0 ended; empty at no letter. */
static void indt_word(Cur *const c, char *const w)
{
    indt_blanks(c);
    uint32_t n = 0;

    while (c->p < c->end && (is_word_char(*c->p) || *c->p == '$')) {
        if (n < WORD - 1)
            w[n++] = (char)upcase(*c->p);

        c->p++;
    }

    w[n] = 0;
}

/* Past the statement: at its ':', or the line's end; false at a remark. */
static bool indt_skip(Cur *const c, uint32_t *const commas)
{
    bool str = false;

    for (; c->p < c->end; c->p++) {
        const uint8_t ch = *c->p;
        str ^= ch == '"';

        if (str)
            continue;
        if (ch == '\'')
            return false;
        if (ch == ':')
            return true;

        *commas += ch == ',';
    }

    return false;
}

/* The statement's last word before the line's end or a remark: THEN, a block IF's. */
static bool indt_then(Cur c)
{
    char w[WORD], last[WORD] = "";

    for (;;) {
        indt_blanks(&c);

        if (c.p >= c.end || *c.p == '\'')
            break;

        indt_word(&c, w);

        if (!strcmp(w, "REM"))
            break;
        if (*w)
            memcpy(last, w, WORD);
        else if (*c.p++ == '"') /* a string, whole */
            while (c.p < c.end && *c.p++ != '"')
                ;
    }

    return !strcmp(last, "THEN");
}

static uint32_t indt_add(Ev *const ev, uint32_t n, const Kind k, const What what, const bool lead)
{
    if (n < NEV)
        ev[n++] = (Ev){k, what, lead};

    return n;
}

/* The line's events, in order; their count. */
static uint32_t indt_scan(const uint8_t *const text, const uint16_t len, Ev *const ev)
{
    static const struct {
        char word[WORD];
        Kind kind;
        What what;
    } plain[] = {
        {"FOR", K_FOR, EV_OPEN},
        {"DO", K_DO, EV_OPEN},
        {"LOOP", K_DO, EV_END},
        {"WHILE", K_WHILE, EV_OPEN},
        {"WEND", K_WHILE, EV_END},
        {"SELECT", K_SEL, EV_OPEN},
        {"CASE", K_SEL, EV_MID},
        {"SUB", K_SUB, EV_OPEN},
        {"FUNCTION", K_FUNC, EV_OPEN},
        {"TYPE", K_TYPE, EV_OPEN},
        {"ELSE", K_IF, EV_MID},
        {"ENDIF", K_IF, EV_END},
    };

    static const struct {
        char word[WORD];
        Kind kind;
    } ends[] = {
        {"IF", K_IF},
        {"SELECT", K_SEL},
        {"SUB", K_SUB},
        {"FUNCTION", K_FUNC},
        {"TYPE", K_TYPE},
        {"DEF", K_DEF},
    };

    Cur c = {text, text + len};

    uint32_t n = 0;
    indt_blanks(&c);

    while (c.p < c.end && is_digit(*c.p)) /* the line's number */
        c.p++;

    for (bool lead = true;; lead = false) {
        char w[WORD];
        indt_word(&c, w);

        if (!strcmp(w, "REM"))
            return n;

        uint32_t commas = 0;

        if (!strcmp(w, "IF") || !strcmp(w, "ELSEIF")) { /* the line's rest is its own */
            if (*w == 'E')
                return indt_add(ev, n, K_IF, EV_MID, lead);

            return indt_then(c) ? indt_add(ev, n, K_IF, EV_OPEN, lead) : n;
        }

        if (!strcmp(w, "END")) {
            char w2[WORD];
            indt_word(&c, w2);

            for (uint32_t i = 0; i < countof(ends); i++)
                if (!strcmp(w2, ends[i].word))
                    n = indt_add(ev, n, ends[i].kind, EV_END, lead);
        } else if (!strcmp(w, "NEXT")) {
            const uint8_t *const at = c.p;
            indt_skip(&c, &commas);
            c.p = at;

            for (uint32_t i = 0; i <= commas; i++)
                n = indt_add(ev, n, K_FOR, EV_END, lead);
        } else if (!strcmp(w, "DEF")) {
            const uint8_t *const at = c.p;
            bool eq = false;

            for (; c.p < c.end && *c.p != ':' && *c.p != '\''; c.p++)
                eq |= *c.p == '=';

            c.p = at;

            if (!eq)
                n = indt_add(ev, n, K_DEF, EV_OPEN, lead);
        } else {
            for (uint32_t i = 0; i < countof(plain); i++)
                if (!strcmp(w, plain[i].word))
                    n = indt_add(ev, n, plain[i].kind, plain[i].what, lead);
        }

        commas = 0;

        if (!indt_skip(&c, &commas))
            return n;

        c.p++; /* past the ':' */
    }
}

static uint16_t indt_lead(const uint8_t *const text, const uint16_t n)
{
    uint16_t i = 0;

    while (i < n && text[i] == ' ')
        i++;

    return i;
}

/* The indent of the start of the block of kind k that line's lead ends, up from line; -1 none. */
static int32_t indt_start(IndtGet *const get, void *const ctx, const uint32_t line, const Kind k)
{
    uint8_t text[LNBUF];
    uint32_t depth = 0;

    for (uint32_t i = line; i--;) {
        const uint16_t len = get(ctx, i, text);
        Ev ev[NEV];

        for (uint32_t e = indt_scan(text, len, ev); e--;) {
            if (ev[e].kind != k || ev[e].what == EV_MID)
                continue;
            if (ev[e].what == EV_END)
                depth++;
            else if (!depth)
                return indt_lead(text, len);
            else
                depth--;
        }
    }

    return -1;
}

void indt_enter(
    IndtGet *const get,
    void *const ctx,
    const uint32_t line,
    const uint8_t *const text,
    const uint16_t n,
    uint16_t *const self,
    uint16_t *const next
)
{
    Ev ev[NEV];
    const uint32_t nev = indt_scan(text, n, ev);
    *self = indt_lead(text, n);

    if (nev && ev[0].lead && ev[0].what != EV_OPEN) { /* an end, a middle: back to its start */
        const int32_t at = indt_start(get, ctx, line, ev[0].kind);

        if (at >= 0)
            *self = (uint16_t)(at + (ev[0].kind == K_SEL && ev[0].what == EV_MID ? INDT_STEP : 0));
    }

    int32_t level = 0;

    for (uint32_t i = 0; i < nev; i++) {
        const bool first = !i && ev[i].lead; /* its block's start's place taken already */

        if (ev[i].what == EV_OPEN || (ev[i].what == EV_MID && first))
            level++;
        else if (ev[i].what == EV_END && !first)
            level--;
    }

    const int32_t in = *self + level * INDT_STEP;
    *next = (uint16_t)(in > 0 ? in : 0);
}
