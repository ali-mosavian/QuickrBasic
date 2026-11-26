/* The editor's auto indent: Enter puts the new line in by the blocks the line opens, and a line
 * starting with a block's end back at the block's start.
 *
 * # How it works
 * A line's block keywords are its dialect's (Dialect.blocks); QBasic's are read here (indt_events).
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
 * | (none) | indt_enter, indt_events | NEW |
 */
#include "ui/indent.h"
#include "ui/ui.h"

/* QBasic's blocks' kinds. */
typedef enum : uint8_t { K_FOR, K_DO, K_WHILE, K_IF, K_SEL, K_SUB, K_FUNC, K_TYPE, K_DEF } Kind;
typedef DialBlock Ev;

enum { NEV = DIAL_NBLOCK, WORD = 12 };

typedef struct {
    const uint8_t *p, *end;
    const uint8_t *word; /* indt_word's word's start */
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
    c->word = c->p;
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

/* Event e, of the keyword from at to the cursor in text: ev's next. */
static uint32_t indt_add(
    Ev *const ev,
    uint32_t n,
    Ev e,
    const bool lead,
    const uint8_t *const text,
    const uint8_t *const at,
    const Cur *const c
)
{
    e.lead = lead;
    e.col = (uint16_t)(at - text);
    e.len = (uint16_t)(c->p - at);

    if (n < NEV)
        ev[n++] = e;

    return n;
}

/* A block IF's opener, a DEF FN's. */
static const Ev if_open = {.kind = K_IF, .what = DBK_OPEN, .end = "END IF"};
static const Ev def_open = {.kind = K_DEF, .what = DBK_OPEN, .end = "END DEF"};

/* QBasic's line's block keywords (indt_blocks). */
static uint32_t indt_events(const uint8_t *const text, const uint16_t len, Ev *const ev)
{
    static const struct {
        char word[WORD];
        Ev ev;
    } plain[] = {
        {"FOR", {.kind = K_FOR, .what = DBK_OPEN, .end = "NEXT"}},
        {"DO", {.kind = K_DO, .what = DBK_OPEN, .end = "LOOP"}},
        {"LOOP", {.kind = K_DO, .what = DBK_END}},
        {"WHILE", {.kind = K_WHILE, .what = DBK_OPEN, .end = "WEND"}},
        {"WEND", {.kind = K_WHILE, .what = DBK_END}},
        {"SELECT", {.kind = K_SEL, .what = DBK_OPEN, .end = "END SELECT"}},
        {"CASE", {.kind = K_SEL, .what = DBK_MID, .inset = true}},
        {"SUB", {.kind = K_SUB, .what = DBK_OPEN, .end = "END SUB"}},
        {"FUNCTION", {.kind = K_FUNC, .what = DBK_OPEN, .end = "END FUNCTION"}},
        {"TYPE", {.kind = K_TYPE, .what = DBK_OPEN, .end = "END TYPE"}},
        {"ELSE", {.kind = K_IF, .what = DBK_MID}},
        {"ENDIF", {.kind = K_IF, .what = DBK_END}},
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

    Cur c = {text, text + len, text};

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
                return indt_add(ev, n, (Ev){.kind = K_IF, .what = DBK_MID}, lead, text, c.word, &c);

            const uint8_t *const at = c.word;
            const Cur word = c;
            return indt_then(c) ? indt_add(ev, n, if_open, lead, text, at, &word) : n;
        }

        const uint8_t *const at = c.word;

        if (!strcmp(w, "END")) {
            char w2[WORD];
            indt_word(&c, w2);

            for (uint32_t i = 0; i < countof(ends); i++)
                if (!strcmp(w2, ends[i].word))
                    n = indt_add(
                        ev,
                        n,
                        (Ev){.kind = ends[i].kind, .what = DBK_END},
                        lead,
                        text,
                        at,
                        &c
                    );
        } else if (!strcmp(w, "NEXT")) {
            const Cur word = c;
            indt_skip(&c, &commas);
            c = word;

            for (uint32_t i = 0; i <= commas; i++)
                n = indt_add(ev, n, (Ev){.kind = K_FOR, .what = DBK_END}, lead, text, at, &word);
        } else if (!strcmp(w, "DEF")) {
            const Cur word = c;
            bool eq = false;

            for (; c.p < c.end && *c.p != ':' && *c.p != '\''; c.p++)
                eq |= *c.p == '=';

            c = word;

            if (!eq)
                n = indt_add(ev, n, def_open, lead, text, at, &word);
        } else {
            for (uint32_t i = 0; i < countof(plain); i++)
                if (!strcmp(w, plain[i].word))
                    n = indt_add(ev, n, plain[i].ev, lead, text, at, &c);
        }

        commas = 0;

        if (!indt_skip(&c, &commas))
            return n;

        c.p++; /* past the ':' */
    }
}

uint32_t indt_blocks(
    const IndtLines *const ls,
    const uint8_t *const text,
    const uint16_t n,
    DialBlock *const ev
)
{
    if (!ls->d->text)
        return indt_events(text, n, ev);

    return ls->d->blocks(ls->f, (const char *)text, n, ev);
}

uint16_t indt_lead(const uint8_t *const text, const uint16_t n)
{
    uint16_t i = 0;

    while (i < n && text[i] == ' ')
        i++;

    return i;
}

/* The indent of the start of the block of kind k that line's lead ends, up from line; -1 none. */
static int32_t indt_start(const IndtLines *const ls, const uint32_t line, const uint8_t k)
{
    uint8_t text[LNBUF];
    uint32_t depth = 0;

    for (uint32_t i = line; i--;) {
        const uint16_t len = ls->get(ls->ctx, i, text);
        Ev ev[NEV];

        for (uint32_t e = indt_blocks(ls, text, len, ev); e--;) {
            if (ev[e].kind != k || ev[e].what == DBK_MID)
                continue;
            if (ev[e].what == DBK_END)
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
    const IndtLines *const ls,
    const uint32_t line,
    const uint8_t *const text,
    const uint16_t n,
    uint16_t *const self,
    uint16_t *const next
)
{
    Ev ev[NEV];
    const uint32_t nev = indt_blocks(ls, text, n, ev);
    *self = indt_lead(text, n);

    if (nev && ev[0].lead && ev[0].what != DBK_OPEN) { /* an end, a middle: back to its start */
        const int32_t at = indt_start(ls, line, ev[0].kind);

        if (at >= 0)
            *self = (uint16_t)(at + (ev[0].inset && ev[0].what == DBK_MID ? INDT_STEP : 0));
    }

    int32_t level = 0;

    for (uint32_t i = 0; i < nev; i++) {
        const bool first = !i && ev[i].lead; /* its block's start's place taken already */

        if (ev[i].what == DBK_OPEN || (ev[i].what == DBK_MID && first))
            level++;
        else if (ev[i].what == DBK_END && !first)
            level--;
    }

    const int32_t in = *self + level * INDT_STEP;
    *next = (uint16_t)(in > 0 ? in : 0);
}
