/* What fits at a place in a line's text, ranked: the editor's completion.
 *
 * # How it works
 * The compiler decides what fits. The line before the word is compiled as the IDE checks a line,
 * with a candidate after it (sugg_fits): the candidate fits when the line compiles, or fails only
 * at its end, wanting more, and not by a Type mismatch. A function's value is checked by its
 * suffix against what a string ("") and a number (0) there give.
 *
 * The candidates are the keywords, and the names of the program's lines in the word's procedure:
 * its variables, the module's SHARED ones and CONSTs from a procedure, the SUBs and FUNCTIONs; a
 * line number where a jump wants one. Each matches the word: its start, or (fuzzy) its letters in
 * order, the first the same. A candidate ranks by how often and how near the word its name is
 * used, a keyword or a name taken lately, and the open FOR's variable after NEXT.
 *
 * Verdicts are kept while the line before the word stays the same: typing on in a word compiles
 * nothing new.
 *
 * # Example
 *   x$ = l     LEFT$( and LCASE$(, not LEN: a string is wanted; a string variable L$ first
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | ac_ctx, ac_mask, ac_want, ac_tkty | sugg_fits: the compiler's verdict | D1 |
 * | ac_vars, ac_ids, ac_label, ac_keys | sugg_scan, sugg_keys | D2 |
 * | ac_open, ac_evs | sugg_events | MIRROR |
 *
 * - D1. The asm guesses what fits from the line's tokens; here the compiler checks each candidate.
 * - D2. The asm takes every name of the window's lines, ten letters at most, by kind then name.
 */
#include "comp/suggest.h"
#include "session.h"
#include "comp/bind.h"
#include "comp/comp.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "comp/lexer.h"
#include "io/dialect.h"
#include "io/gen.h"
#include "mem/module.h"
#include "mem/program.h"
#include "mem/scope.h"
#include "vm/flow.h"
#include "util/grow.h"
#include "util/hash.h"

enum {
    NNAMES = 512,   /* the names kept of a scan */
    NHASH = 256,    /* their hash chains */
    NVERDICT = 512, /* the verdicts kept for one line before the word */
    NRECENT = 16,   /* the suggestions taken lately */
    NFOR = 16,      /* open FORs kept */
    NPROCS = 128,   /* procedures kept */
    TEXT_MAX = 600, /* a probe's text */
};

/* A name of the program: as typed, in capitals, an array's '(' after it. */
typedef struct {
    uint8_t len;
    uint8_t text[SUGG_LEN + 2];
    uint32_t name;  /* its entry in the name table */
    uint32_t scope; /* its procedure's name, 0 the module */
    uint16_t count; /* its uses */
    uint32_t dist;  /* the nearest use's lines from the word */
    int16_t next;   /* its hash chain's next, -1 none */
} SuggName;

/* A candidate after the line before the word: no fit, a fit wanting more, a fit. */
typedef enum : uint8_t { PROBE_NO, PROBE_MORE, PROBE_DONE } Probe;

typedef struct {
    uint32_t hash;
    Probe probe;
} Verdict;

/* What a suggestion is found with: the compiler, the lexer, the program and its scopes. */
typedef struct {
    Comp *comp;
    Lexr *lexr;
    const Prog *prog;
    const Scop *scop;
    const Decl *decl; /* the TYPEs */
    Symb *symb;       /* the names, found by their texts */
    const Modu *modu; /* the lines' modules */
    uint32_t module;  /* the word's */
} SuggSrc;

/* Is line ln the word's module's, QBasic's? */
static bool sugg_ours(const SuggSrc *const src, const Line *const ln)
{
    return !(ln->flags & LF_TEXT) && modu_of(src->modu, ln->file) == src->module;
}

static struct {
    uint8_t before[TEXT_MAX]; /* the line before the word */
    bool either;              /* after THEN or ELSE: a line number or a statement */
    bool as;                  /* after AS: a type, the program's TYPEs too */
    bool hash;                /* after '#': a file's number */
    uint16_t nbefore;
    bool broken;  /* it fails before its end: no verdicts */
    bool str_ok;  /* a string fits after it */
    bool num_ok;  /* a number fits after it */
    bool jump;    /* a line number alone fits: a jump's target */
    uint8_t last; /* its last statement's keyword */
    uint8_t text[TEXT_MAX];
    uint8_t body[TEXT_MAX];
    Verdict *verdict; /* NVERDICT, made at the first find */
    SuggName *names;  /* ncap, grown to NNAMES */
    uint16_t nnames;
    uint32_t ncap;
    int16_t head[NHASH];
    uint8_t recent[NRECENT][SUGG_LEN + 2];
    uint8_t nrecent;
    uint32_t procs[NPROCS]; /* the procedures' names */
    uint16_t nprocs;
    SuggName open[NFOR]; /* the open FORs' variables, innermost last */
    uint8_t nopen;
    uint8_t ndo, nwhile, nsel; /* DO, WHILE, SELECT CASE open at the word */
} sg;

static uint32_t sugg_hash(const uint8_t *const s, const uint32_t n) { return fnv(FNV_SEED, s, n); }

/* ---- the compiler's verdict */

/* sg.text compiled alone, as the IDE checks a line: true an error, its body offset and code;
   *len the body's bytes. */
static bool sugg_compile(
    Comp *const C,
    Lexr *const lx,
    uint32_t *const len,
    uint32_t *const at,
    uint8_t *const err
)
{
    const uint8_t *const end = lexr_line(lx, sg.text, C->cbuf);
    *len = (uint32_t)(end - C->cbuf - 1);
    C->check = true;
    (void)comp_line(C);
    C->check = false;

    for (const uint8_t *op = C->xbuf; *op != O_EOL; op += flow_opsize(op)) {
        if (exec_is_err(op)) {
            const ErrOp *const e = (const ErrOp *)op;
            *at = e->at;
            *err = e->code;
            return true;
        }
    }

    return false;
}

/* The line before the word, then n characters at s, to sg.text. */
static bool sugg_compose(const uint8_t *const s, const uint32_t n)
{
    if (sg.nbefore + n >= TEXT_MAX)
        return false;

    memcpy(sg.text, sg.before, sg.nbefore);
    memcpy(sg.text + sg.nbefore, s, n);
    sg.text[sg.nbefore + n] = 0;
    return true;
}

/* The n characters at s after the line before the word: it compiles (done), or fails at its end
   (more), not by a Type mismatch. */
static Probe sugg_probe(const SuggSrc *const src, const uint8_t *const s, const uint32_t n)
{
    if (sg.broken)
        return PROBE_DONE;

    const uint32_t h = sugg_hash(s, n);
    Verdict *const v = &sg.verdict[h % NVERDICT];

    if (v->hash == h)
        return v->probe;
    if (!sugg_compose(s, n))
        return PROBE_NO;

    uint32_t len, at;
    uint8_t err;
    Probe p = PROBE_DONE;

    if (sugg_compile(src->comp, src->lexr, &len, &at, &err))
        p = err != E_TM && at >= len ? PROBE_MORE : PROBE_NO;

    *v = (Verdict){h, p};

    return p;
}

static bool sugg_fits(const SuggSrc *const src, const uint8_t *const s, const uint32_t n)
{
    return sugg_probe(src, s, n) != PROBE_NO;
}

/* ---- the line before the word */

/* Its last statement's keyword, and whether a jump's line number fits after it. */
static void sugg_tail(const uint8_t *const body, const uint32_t len)
{
    const uint8_t *p = body;
    const uint8_t *last = nullptr; /* its last item */
    bool on = false;
    sg.last = 0;

    for (uint8_t c; p < body + len && (c = *p);) {
        if (c == ':') {
            on = false;
            sg.last = 0;
        } else if (!sg.last && c >= TK_END)
            sg.last = c;

        if (c == TK_ON)
            on = true;

        last = p;
        p = decl_step(p);
    }

    const uint8_t c = last ? *last : 0;
    const bool resume = c == TK_X && last[1] == TK1_RESUME;
    sg.either = c == TK_THEN || c == TK_ELSE;
    sg.as = c == TK_AS;
    sg.hash = c == '#';
    sg.as = c == TK_AS;
    sg.hash = c == '#';
    sg.jump = c == TK_GOTO || c == TK_GOSUB || c == TK_RUN || c == TK_LIST || c == TK_RES ||
              resume || (on && c == ',');
}

/* The line before the word is text's first n characters: compiled alone, and with a string and a
   number after it, when it changed. */
static void sugg_before(const SuggSrc *const src, const uint8_t *const text, const uint16_t n)
{
    if (n == sg.nbefore && !memcmp(text, sg.before, n))
        return;

    memcpy(sg.before, text, n);
    sg.nbefore = n;
    memset(sg.verdict, 0xFF, NVERDICT * sizeof *sg.verdict);
    sg.broken = false;
    sugg_compose((const uint8_t *)"", 0);
    uint32_t len, at;
    uint8_t err;
    sg.broken = sugg_compile(src->comp, src->lexr, &len, &at, &err) && at < len;
    sg.str_ok = !sg.broken && sugg_fits(src, (const uint8_t *)"\"\"", 2);
    sg.num_ok = !sg.broken && sugg_fits(src, (const uint8_t *)"0", 1);
}

/* ---- matching and ranking */

/* word (n letters, capitals) against s: 2 its start, 1 its letters in order from the first, 0
   neither; s longer than the word. */
static uint8_t sugg_match(
    const uint8_t *const word,
    const uint16_t n,
    const uint8_t *const s,
    const uint16_t len
)
{
    if (len <= n)
        return 0;
    if (!memcmp(s, word, n))
        return 2;
    if (!n || s[0] != word[0])
        return 0;

    uint16_t i = 1;

    for (uint16_t j = 1; j < len && i < n; j++)
        if (s[j] == word[i])
            i++;

    return i == n ? 1 : 0;
}

/* The bonus of a suggestion taken lately: the latest most. */
static int16_t sugg_recency(const uint8_t *const s)
{
    for (uint8_t i = 0; i < sg.nrecent; i++)
        if (!strcmp((const char *)sg.recent[i], (const char *)s))
            return (int16_t)(60 - 3 * i);

    return 0;
}

/* a before b: a start's match first, then by score, the shorter, the name. */
static bool sugg_before_in(const Sugg *const a, const Sugg *const b)
{
    if (a->fuzzy != b->fuzzy)
        return !a->fuzzy;
    if (a->score != b->score)
        return a->score > b->score;
    if (a->len != b->len)
        return a->len < b->len;

    return strcmp((const char *)a->text, (const char *)b->text) < 0;
}

/* s to the list in its place, once; the last dropped when full. */
static void sugg_add(Suggs *const out, const Sugg *const s)
{
    for (uint16_t i = 0; i < out->n; i++)
        if (!strcmp((const char *)out->list[i].text, (const char *)s->text))
            return;

    uint16_t i = 0;

    while (i < out->n && !sugg_before_in(s, &out->list[i]))
        i++;

    /* the list's most: SUGG_MAX, those it holds when it cannot grow */
    const uint16_t most =
        out->n >= SUGG_MAX || GROW(out->list, out->cap, out->n + 1u) ? SUGG_MAX : out->n;

    if (i == most)
        return;

    const uint16_t keep = out->n < most ? out->n : most - 1;
    memmove(&out->list[i + 1], &out->list[i], (keep - i) * sizeof(Sugg));
    out->list[i] = *s;
    out->n = keep + 1;
}

/* A suggestion of text (len, capitals), its match against the word. */
static Sugg sugg_make(
    const uint8_t *const text,
    const uint16_t len,
    const SuggKind kind,
    const uint8_t match,
    const int16_t score
)
{
    Sugg s = {.len = (uint8_t)len, .kind = kind, .fuzzy = match == 1};

    memcpy(s.text, text, len);
    s.text[len] = 0;
    s.score = (int16_t)(score + sugg_recency(s.text));
    return s;
}

/* sugg_make's for name id: its letters as first written. */
static Sugg sugg_named(
    const SuggSrc *const src,
    const uint32_t id,
    const uint8_t *const text,
    const uint16_t len,
    const SuggKind kind,
    const uint8_t match,
    const int16_t score
)
{
    uint8_t shown[SUGG_LEN + 2];
    memcpy(shown, text, len);
    symb_spell(src->symb, id, shown, (uint8_t)len);
    Sugg s = sugg_make(shown, len, kind, match, score);
    s.asis = s.len;
    return s;
}

/* ---- the keywords */

/* The keyword at p in a keyword list to w (capitals, 0-ended), *n its length; past it. */
static const uint8_t *sugg_kw(const uint8_t *const p, uint8_t *const w, uint16_t *const n)
{
    uint16_t i = 0;

    for (; !(p[i] & 0x80); i++)
        w[i] = p[i];

    w[i] = p[i] & 0x7F;
    w[i + 1] = 0;
    *n = i + 1;
    return p + i + 1;
}

/* Token tok a function by its form: '(' after it. */
static bool sugg_isfn(const uint16_t tok)
{
    return (tok >= TK_SGN && tok <= TK_MID) || tok == TK_TAB || tok == TK_SPC ||
           (tok >= TK_LBOUND && tok < TK_X);
}

/* The line before the word with n characters at s, lexed: its last item's first byte. */
static uint8_t sugg_lexed(const SuggSrc *const src, const uint8_t *const s, const uint16_t n)
{
    if (!sugg_compose(s, n))
        return 0;

    const uint8_t *const buf = src->comp->cbuf;
    const uint8_t *const end = lexr_line(src->lexr, sg.text, src->comp->cbuf) - 1;
    const uint8_t *last = buf;

    for (const uint8_t *p = buf; p < end && *p; p = decl_step(p))
        last = p;

    return *last;
}

/* Keyword w (n characters) of token tok, matched: to the list when it fits. A function, or one
   wanting more, in an operand gives what its suffix says. */
static void sugg_keyword(
    const SuggSrc *const src,
    Suggs *const out,
    const uint8_t *const w,
    const uint16_t n,
    const uint16_t tok,
    const uint8_t match
)
{
    const Probe bare = sugg_probe(src, w, n);

    if (bare == PROBE_NO || sugg_lexed(src, w, n) == M_VAR) /* there a name, not the keyword */
        return;

    const bool operand = sg.str_ok || sg.num_ok;
    bool fn = false;

    if (operand && bare == PROBE_MORE) {
        uint8_t call[SUGG_LEN + 2];
        memcpy(call, w, n);
        call[n] = '(';
        fn = tok < 0x100 ? sugg_isfn(tok) : sugg_fits(src, call, n + 1u);

        if (!(w[n - 1] == '$' ? sg.str_ok : sg.num_ok))
            return;
    }

    const bool closes = (tok == TK_LOOP && sg.ndo) || (tok == TK_WEND && sg.nwhile) ||
                        (tok == TK_CASE && sg.nsel) || (tok == TK_NEXT && sg.nopen);
    Sugg s = sugg_make(
        w,
        n,
        fn ? SG_FN : SG_KW,
        match,
        (int16_t)((fn        ? 15
                   : operand ? 10
                             : 40) +
                  (closes ? 30 : 0))
    );
    s.tok = tok;
    sugg_add(out, &s);
}

/* The keywords that match the word (n letters) and fit. */
static void sugg_keys(
    const SuggSrc *const src,
    Suggs *const out,
    const uint8_t *const word,
    const uint16_t n
)
{
    static const struct {
        const uint8_t *list;
        uint16_t first;
    } lists[] = {{cons_kwlist, TK_END}, {cons_kwlist1, 0x100}};

    uint8_t w[64];

    for (uint32_t l = 0; l < countof(lists); l++) {
        const uint8_t *p = lists[l].list;

        for (uint16_t tok = lists[l].first; *p; tok++) {
            uint16_t len;
            p = sugg_kw(p, w, &len);
            const uint8_t m = is_letter(w[0]) ? sugg_match(word, n, w, len) : 0;

            if (m && len <= SUGG_LEN)
                sugg_keyword(src, out, w, len, tok, m);
        }
    }
}

/* ---- punctuation */

/* The word (wn capitals) a keyword's or a name's, whole. */
static bool sugg_known(const uint8_t *const word, const uint16_t wn)
{
    for (uint16_t i = 0; i < sg.nnames; i++) {
        const SuggName *const e = &sg.names[i];
        const uint16_t len = e->len - (e->text[e->len - 1] == '(');

        if (len == wn && !memcmp(e->text, word, wn))
            return true;
    }

    const uint8_t *const lists[] = {cons_kwlist, cons_kwlist1};

    uint8_t w[64];

    for (uint32_t l = 0; l < countof(lists); l++) {
        for (const uint8_t *p = lists[l]; *p;) {
            uint16_t len;
            p = sugg_kw(p, w, &len);

            if (len == wn && !memcmp(w, word, wn))
                return true;
        }
    }

    return false;
}

/* The '(' open in the line before the word, outside its strings. */
static int32_t sugg_depth(void)
{
    int32_t depth = 0;
    bool str = false;

    for (uint16_t i = 0; i < sg.nbefore; i++) {
        const uint8_t c = sg.before[i];
        str ^= c == '"';
        depth += !str && c == '(' ? 1 : !str && c == ')' ? -1 : 0;
    }

    return depth;
}

/* The line before the word's last character, past its spaces; 0 none. */
static uint8_t sugg_last(void)
{
    uint16_t n = sg.nbefore;

    while (n && sg.before[n - 1] == ' ')
        n--;

    return n ? sg.before[n - 1] : 0;
}

static bool sugg_suffix(const uint8_t c)
{
    return c == '$' || c == '%' || c == '&' || c == '!' || c == '#';
}

static int32_t sugg_left(const SuggSrc *src);

/* What goes on after the word (wn characters at typed, wn capitals at word): a mark right after
   it, a joining keyword after a space; the word with it. One the statement needs, SUGG_NEED: it
   ends the statement (FOR's TO, IF's THEN), or nothing else lets it go on (FOR i's '='). A word
   being typed gets them only when nothing else starts with it; a name's '(' comes with it. */
static void sugg_follow(
    const SuggSrc *const src,
    Suggs *const out,
    const uint8_t *const typed,
    const uint8_t *const word,
    const uint16_t wn
)
{
    static const char *const follows[] = {
        "(",  ")",   ",",   ";",   "=",   ":",  "TO",   "STEP",  "THEN", "ELSE",  "AND",
        "OR", "XOR", "MOD", "EQV", "IMP", "AS", "GOTO", "GOSUB", "IS",   "USING", "#",
    };

    if (wn && out->n && !sugg_known(word, wn)) /* a word being typed */
        return;

    bool longer = false; /* a word the typed one starts: it may be that word's start */

    for (uint16_t i = 0; i < out->n; i++)
        longer |= !out->list[i].fuzzy && out->list[i].len > wn;

    const bool open = sugg_probe(src, typed, wn) == PROBE_MORE;       /* the statement wants more */
    const bool shut = sugg_depth() > 0 && (wn || sugg_last() != '('); /* its ')', not "()" */
    /* a call's parameters still to come; a header being written: its own count */
    const bool header = sg.last == TK_SUB || sg.last == TK_FUNCTION || sg.last == TK_DECLARE;
    const int32_t left = header ? -1 : sugg_left(src);
    const bool known = wn && sugg_known(word, wn);
    Sugg fit[countof(follows)];
    uint32_t nfit = 0, nmore = 0; /* those fitting; of them, those the statement goes on by */
    bool ended = false;

    for (uint32_t i = 0; i < countof(follows); i++) {
        if (wn && out->n && follows[i][0] == '(') /* it comes with the name */
            continue;

        const uint16_t fn = (uint16_t)strlen(follows[i]);
        const bool mark = fn == 1 && !is_letter((uint8_t)follows[i][0]) && follows[i][0] != '#';
        const uint16_t gap = !mark && wn ? 1 : 0;
        const uint16_t pn = wn + gap + fn;

        if (pn > SUGG_LEN)
            continue;

        uint8_t probe[SUGG_LEN + 4]; /* its text, then " 0" after it */
        memcpy(probe, typed, wn);
        probe[wn] = ' ';
        memcpy(probe + wn + gap, follows[i], fn);

        if ((left > 0 && follows[i][0] == ')') || (!left && follows[i][0] == ','))
            continue; /* a call of the program's: its parameters' count */
        if (!sugg_fits(src, probe, pn))
            continue;

        memcpy(probe + pn, " 0", 2);
        const bool close = shut && left <= 0 && follows[i][0] == ')';
        const bool more = left > 0 && follows[i][0] == ',';
        const bool ends =
            close || more || (open && !longer && sugg_probe(src, probe, pn + 2u) == PROBE_DONE);
        /* needed: first by the list's order, THEN before GOTO; a word it may start: after it */
        const int16_t score = ends                   ? (int16_t)(SUGG_NEED + countof(follows) - i)
                              : longer               ? 0
                              : follows[i][0] == ')' ? 65
                              : follows[i][0] != '(' ? 60
                              : known                ? 65
                                                     : 55;
        ended |= ends;
        nmore += follows[i][0] != '(';
        fit[nfit] = sugg_make(probe, pn, SG_PUNCT, 2, score);
        fit[nfit].asis = (uint8_t)wn;
        fit[nfit++].need = ends;
    }

    /* a parameter list being declared: another parameter, more likely than its end */
    Sugg *comma = nullptr;

    for (uint32_t i = 0; i < nfit; i++)
        comma = fit[i].text[wn] == ',' ? &fit[i] : comma;

    for (uint32_t i = 0; i < nfit && header && shut && comma; i++) {
        if (fit[i].text[wn] == ')' && fit[i].need) {
            comma->score = fit[i].score;
            comma->need = true;
            fit[i].score = 65;
            fit[i].need = false;
        }
    }

    for (uint32_t i = 0; i < nfit; i++) {
        if (open && !longer && !ended && nmore == 1 && fit[i].text[wn] != '(') {
            fit[i].score = SUGG_NEED;
            fit[i].need = true;
        }

        sugg_add(out, &fit[i]);
    }
}

/* ---- the program's names */

/* The M_VAR at p in capitals to *n, an array's '(' after it: false too long. Past it. */
static bool sugg_name(const uint8_t *const p, SuggName *const n, const uint8_t **const past)
{
    const uint32_t len = p[1];
    const uint8_t *const next = comp_skipname(p);
    *past = next;
    const uint32_t total = len + (*next == '(');

    if (total > SUGG_LEN)
        return false;

    for (uint32_t i = 0; i < len; i++)
        n->text[i] = upcase(p[2 + i]);
    if (*next == '(')
        n->text[len] = '(';

    n->text[total] = 0;
    n->len = (uint8_t)total;
    n->name = comp_nameof(p);
    return true;
}

/* Name n's use in scope, dist lines from the word: a new entry, or its count up. */
static void sugg_use(const SuggName *const n, const uint32_t scope, const uint32_t dist)
{
    const uint32_t h = sugg_hash(n->text, n->len) % NHASH;

    for (int16_t i = sg.head[h]; i >= 0; i = sg.names[i].next) {
        SuggName *const e = &sg.names[i];

        if (e->scope == scope && e->len == n->len && !memcmp(e->text, n->text, n->len)) {
            e->count++;

            if (dist < e->dist)
                e->dist = dist;

            return;
        }
    }

    if (sg.nnames == NNAMES || !GROW(sg.names, sg.ncap, sg.nnames + 1u))
        return;

    SuggName *const e = &sg.names[sg.nnames];
    *e = *n;
    e->scope = scope;
    e->count = 1;
    e->dist = dist;
    e->next = sg.head[h];
    sg.head[h] = (int16_t)sg.nnames++;
}

/* The names of body p (to its end, len bytes; 0-ended when len is 0), in scope. */
static void sugg_names(
    const uint8_t *const body,
    const uint32_t len,
    const uint32_t scope,
    const uint32_t dist
)
{
    const uint8_t *p = body;
    uint8_t prev = 0;

    for (uint8_t c; (len ? p < body + len : true) && (c = *p) && c != TK_REM && c != TK_DATA;) {
        if (c != M_VAR || prev == '.' ||
            prev == TK_AS) { /* r.f's field, AS t's TYPE: no variable */
            prev = c;
            p = decl_step(p);
            continue;
        }

        SuggName n;
        prev = c;

        if (sugg_name(p, &n, &p))
            sugg_use(&n, scope, dist);
    }
}

/* ---- FOR and NEXT */

/* Body p's FORs and NEXTs on sg.open; till end. A NEXT ending it, naming none yet, is the one being
   written: it closes nothing. */
static void sugg_events(const uint8_t *const body, const uint8_t *const end)
{
    const uint8_t *p = body;
    bool start = true; /* at a statement's start */

    for (uint8_t c; p < end && (c = *p) && c != TK_REM && c != TK_DATA;) {
        p = decl_step(p);
        const bool first = start;
        start = c == ':';

        if (c == TK_DO)
            sg.ndo++;
        else if (c == TK_LOOP && sg.ndo)
            sg.ndo--;
        else if (c == TK_WHILE && first)
            sg.nwhile++;
        else if (c == TK_WEND && sg.nwhile)
            sg.nwhile--;
        else if (c == TK_SELECT)
            sg.nsel++;
        else if (c == TK_END && p < end && *p == TK_SELECT) {
            p = decl_step(p);
            sg.nsel -= sg.nsel > 0;
        }

        if (c == TK_FOR && *p == M_VAR) {
            SuggName n;

            if (sugg_name(p, &n, &p) && sg.nopen < NFOR)
                sg.open[sg.nopen++] = n;
        } else if (c == TK_NEXT) {
            uint8_t k = 0;

            for (; p < end && (*p == M_VAR || *p == ','); p = decl_step(p))
                k += *p == M_VAR;

            const uint8_t closed = k ? k : (p < end && *p ? 1 : 0);
            sg.nopen = closed < sg.nopen ? sg.nopen - closed : 0;
        }
    }
}

/* ---- a scan of the program */

/* n in decimal to s; its digits. */
static uint16_t sugg_decimal(const uint16_t n, uint8_t *const s)
{
    uint8_t d[5];
    uint16_t k = 0;

    for (uint16_t v = n; k == 0 || v; v /= 10)
        d[k++] = (uint8_t)('0' + v % 10);
    for (uint16_t i = 0; i < k; i++)
        s[i] = d[k - 1 - i];

    s[k] = 0;
    return k;
}

/* After AS: the program's TYPEs the word starts. */
static void sugg_types(
    const SuggSrc *const src,
    Suggs *const out,
    const uint8_t *const word,
    const uint16_t wn
)
{
    for (uint16_t t = 1; t <= src->decl->ntypes; t++) {
        const uint32_t id = decl_type(src->decl, t)->name;
        const uint8_t *const text = symb_text(src->symb, id);
        const uint8_t m = sugg_match(word, wn, text + 1, text[0]);

        if (m && sugg_fits(src, text + 1, text[0])) {
            const Sugg s = sugg_named(src, id, text + 1, text[0], SG_VAR, m, 45);
            sugg_add(out, &s);
        }
    }
}

/* The file numbers the program's lines name after a '#', that the word starts and fit; the
   program's one file needed. */
static void sugg_files(
    const SuggSrc *const src,
    Suggs *const out,
    const uint8_t *const word,
    const uint16_t wn
)
{
    const Prog *const pg = src->prog;
    uint16_t nums[16];
    uint32_t n = 0;

    for (uint32_t rec = PROGBASE; prog_line(pg, rec)->len; rec += prog_line(pg, rec)->len) {
        if (prog_line(pg, rec)->flags & LF_ASIS)
            continue;

        for (const uint8_t *p = at(prog_addr(pg, rec + HDR)); *p; p = decl_step(p)) {
            if (*p != '#' || p[1] != M_INT)
                continue;

            const uint16_t v = (uint16_t)load32(p + 2);
            uint32_t k = 0;

            while (k < n && nums[k] != v)
                k++;

            if (k == n && n < countof(nums))
                nums[n++] = v;
        }
    }

    for (uint32_t k = 0; k < n; k++) {
        uint8_t digits[8];
        const uint16_t len = sugg_decimal(nums[k], digits);
        const uint8_t m = sugg_match(word, wn, digits, len);

        if ((m || !wn) && sugg_fits(src, digits, len)) {
            Sugg s = sugg_make(digits, len, SG_LABEL, m ? m : 2, 80);
            s.need = n == 1 && !wn;
            sugg_add(out, &s);
        }
    }
}

/* Where the word is: its line's ordinal, its procedure, and what the scan found of it. */
typedef struct {
    uint32_t rec;   /* the word's line */
    uint32_t ord;   /* its ordinal, past the last when none */
    uint32_t scope; /* its procedure's name, 0 the module */
    uint8_t prior;  /* the line before's last statement's keyword */
} SuggAt;

/* Pass one: the word's line and procedure, the open FORs before it. */
static void sugg_where(const SuggSrc *const src, SuggAt *const w)
{
    const Prog *const pg = src->prog;
    uint32_t scope = 0;
    uint8_t prior = 0;
    sg.nopen = sg.ndo = sg.nwhile = sg.nsel = 0;
    uint32_t ord = 0;

    for (uint32_t rec = PROGBASE; prog_line(pg, rec)->len; rec += prog_line(pg, rec)->len, ord++) {
        const Line *const ln = prog_line(pg, rec);
        const uint8_t *const body = at(prog_addr(pg, rec + HDR));
        const uint8_t *name;
        const bool ours = sugg_ours(src, ln) && !(ln->flags & LF_ASIS);
        const uint8_t kind = ours ? decl_proc(body, &name) : 0;

        if (kind == PK_SUB || kind == PK_FUNC) {
            scope = comp_nameof(name);
            sg.nopen = sg.ndo = sg.nwhile = sg.nsel = 0;
        }

        if (rec == w->rec) {
            *w = (SuggAt){rec, ord, scope, prior};

            return;
        }

        if (ours)
            sugg_events(body, body + ln->len - HDR);
        if (kind == PK_END) {
            scope = 0;
            sg.nopen = sg.ndo = sg.nwhile = sg.nsel = 0;
        }

        const uint8_t *t = body;
        decl_tail(&t);
        prior = !sugg_ours(src, ln) ? prior : ln->flags & LF_ASIS ? 0 : *t;
    }

    *w = (SuggAt){w->rec, ord, scope, prior};
}

/* A name of scope seen from the word's procedure: its own, or the module's SHARED or CONST. */
static bool sugg_seen(const SuggSrc *const src, const SuggName *const n, const uint32_t from)
{
    if (n->scope == from)
        return true;
    if (n->scope)
        return false;

    const Binding *const b = scop_get(src->scop, sc_module(src->module), n->name);
    return b && b->flags & (BF_SHR | BF_CON);
}

/* Pass two: the names, the procedures and the line numbers that match the word (n letters). */
/* The before text ends in the keyword kw (4 letters), a word of its own, as typed. */
static bool sugg_endsin(const uint16_t n, const char *const kw)
{
    if (n < 4 || (n > 4 && is_letter(sg.before[n - 5])))
        return false;

    for (uint16_t i = 0; i < 4; i++)
        if ((sg.before[n - 4 + i] & ~0x20) != kw[i])
            return false;

    return true;
}

/* The program's SUB or FUNCTION of n capitals at name: its parameters; -1 none. */
static int32_t sugg_params(const SuggSrc *const src, const uint8_t *const name, const uint16_t n)
{
    const Prog *const pg = src->prog;

    for (uint32_t rec = PROGBASE; prog_line(pg, rec)->len; rec += prog_line(pg, rec)->len) {
        const uint8_t *const body = at(prog_addr(pg, rec + HDR));
        const uint8_t *nm, *p;
        SuggName sn;
        const uint8_t kind = decl_proc(body, &nm);

        if ((kind != PK_SUB && kind != PK_FUNC) || !sugg_name(nm, &sn, &p))
            continue;
        if (sn.len - (*p == '(') != n || memcmp(sn.text, name, n))
            continue;
        if (*p != '(')
            return 0;

        p = decl_step(p);
        int32_t count = *p != ')';

        for (uint32_t depth = 1; *p && depth; p = decl_step(p)) {
            depth += *p == '(' ? 1 : *p == ')' ? -1 : 0;
            count += depth == 1 && *p == ',';
        }

        return count;
    }

    return -1;
}

/* The innermost '(' open in the line before the word, its index; -1 none. */
static int32_t sugg_paren(void)
{
    int32_t stack[16];
    uint32_t depth = 0;
    bool str = false;

    for (uint16_t i = 0; i < sg.nbefore; i++) {
        const uint8_t c = sg.before[i];
        str ^= c == '"';

        if (str)
            continue;
        if (c == '(' && depth < countof(stack))
            stack[depth++] = i;
        else if (c == ')' && depth)
            depth--;
    }

    return depth ? stack[depth - 1] : -1;
}

/* The name before the '(' at open in the line before the word, in capitals to name; its length,
   0 none. */
static uint16_t sugg_callee(const int32_t open, uint8_t *const name)
{
    int32_t e = open;

    while (e > 0 && sg.before[e - 1] == ' ')
        e--;

    int32_t b = e;

    while (b > 0 && (is_word_char(sg.before[b - 1]) || sugg_suffix(sg.before[b - 1])))
        b--;

    if (open < 0 || b == e || e - b > SUGG_LEN)
        return 0;

    for (int32_t i = b; i < e; i++)
        name[i - b] = upcase(sg.before[i]);

    name[e - b] = 0;
    return (uint16_t)(e - b);
}

/* The statement the word is in: a name and a blank, its arguments after: the name in capitals to
   name, its length (0 none), *open the blank's index. */
static uint16_t sugg_bare(int32_t *const open, uint8_t *const name)
{
    int32_t s = 0;
    bool str = false;

    for (int32_t i = 0; i < sg.nbefore; i++) { /* past the statements before it */
        str ^= sg.before[i] == '"';
        s = !str && sg.before[i] == ':' ? i + 1 : s;
    }

    while (s < sg.nbefore && sg.before[s] == ' ')
        s++;

    int32_t e = s;

    while (e < sg.nbefore && is_word_char(sg.before[e]))
        e++;

    if (e == s || e >= sg.nbefore || sg.before[e] != ' ' || e - s > SUGG_LEN)
        return 0;

    for (int32_t i = s; i < e; i++)
        name[i - s] = upcase(sg.before[i]);

    *open = e;
    return (uint16_t)(e - s);
}

/* A call of the program's procedures open at the word: its parameters after the word's
   argument; -1 none, or not one of the program's. */
static int32_t sugg_left(const SuggSrc *const src)
{
    int32_t open = sugg_paren();
    uint8_t name[SUGG_LEN + 1];
    uint16_t nn = sugg_callee(open, name);

    if (!nn && open < 0) /* a SUB's call without CALL: its name, a space, its arguments */
        nn = sugg_bare(&open, name);
    if (!nn)
        return -1;

    uint32_t arg = 0, depth = 0;
    bool str = false;

    for (uint16_t i = (uint16_t)open + 1u; i < sg.nbefore; i++) {
        const uint8_t c = sg.before[i];
        str ^= c == '"';
        depth += !str && c == '(' ? 1 : !str && c == ')' ? -1 : 0;
        arg += !str && !depth && c == ',';
    }

    const int32_t n = sugg_params(src, name, nn);
    return n < 0 || (int32_t)arg >= n ? -1 : n - 1 - (int32_t)arg;
}

/* Past the ( .. ) ending before b[e] backwards: the '(''s index; -1 none matched. */
static int32_t sugg_back(const uint8_t *const b, int32_t e)
{
    for (int32_t depth = 0; e > 0;) {
        const uint8_t c = b[--e];
        depth += c == ')' ? 1 : c == '(' ? -1 : 0;

        if (!depth)
            return e;
    }

    return -1;
}

/* Past the ( .. ) at b[i], before e. */
static int32_t sugg_parens(const uint8_t *const b, int32_t i, const int32_t e)
{
    int32_t depth = 0;

    do
        depth += b[i] == '(' ? 1 : b[i] == ')' ? -1 : 0;
    while (++i < e && depth);

    return i;
}

/* The TYPE of the record the line before the word ends in, a '.' after it: a variable's, an
   element's, down its fields' path, a fixed array field's element ours; 0 none. */
static uint32_t sugg_record(const SuggSrc *const src)
{
    const uint8_t *const b = sg.before;
    const int32_t e = sg.nbefore - 1;

    if (e < 1 || b[e] != '.')
        return 0;

    int32_t s = e; /* the reference's start: names, dots, ( .. ) */

    while (s > 0) {
        const uint8_t c = b[s - 1];

        if (c == ')')
            s = sugg_back(b, s);
        else if (is_name_char(c) || sugg_suffix(c))
            s--;
        else
            break;

        if (s < 0)
            return 0;
    }

    int32_t i = s;

    while (i < e && is_word_char(b[i]))
        i++;

    if (i == s || !is_letter(b[s]))
        return 0;

    const uint32_t name = symb_find_name(src->symb, 0, b + s, (uint8_t)(i - s));
    const bool arr = b[i] == '(';

    if (arr)
        i = sugg_parens(b, i, e);

    uint32_t t = scop_rec(src->scop, name, arr);

    while (t && i < e && b[i] == '.') { /* .f: its TYPE's field, a record */
        const int32_t f0 = ++i;

        while (i < e && is_word_char(b[i]))
            i++;

        const uint32_t fn = symb_find_name(src->symb, 0, b + f0, (uint8_t)(i - f0));
        const TypeDef *const td = decl_type(src->decl, t);
        const Field *f = nullptr;

        for (uint32_t k = 0; k < td->nfld; k++)
            if (td->fields[k].name == fn)
                f = &td->fields[k];

        t = f ? f->sub : 0;

        if (f && (f->ndim || f->dyn) && i < e && b[i] == '(')
            i = sugg_parens(b, i, e);
    }

    return i == e ? t : 0;
}

/* After a record's '.': its fields the word starts, that fit; false not after one. */
static bool sugg_fields(
    const SuggSrc *const src,
    Suggs *const out,
    const uint8_t *const word,
    const uint16_t wn
)
{
    const uint32_t t = sugg_record(src);

    if (!t)
        return false;

    const TypeDef *const td = decl_type(src->decl, t);
    sg.broken = false; /* the line before, ending in its '.', fails alone: each field checked */

    for (uint32_t k = 0; k < td->nfld; k++) {
        const uint32_t id = td->fields[k].name;
        const uint8_t *const text = symb_text(src->symb, id);
        const uint8_t m = sugg_match(word, wn, text + 1, text[0]);

        if (m && sugg_fits(src, text + 1, text[0])) {
            const Sugg s = sugg_named(src, id, text + 1, text[0], SG_VAR, m, 50);
            sugg_add(out, &s);
        }
    }

    return true;
}

/* sugg_exports': where the word's matches go. */
typedef struct {
    const SuggSrc *src;
    Suggs *out;
    const uint8_t *word;
    uint16_t wn;
} SuggExports;

/* Export t, its kinds k, offered when it matches the word; not a TYPE the module's importers
   can't use. */
static void sugg_export(void *const ctx, const uint32_t t, const uint8_t k)
{
    const SuggExports *const q = ctx;
    const Name *const nm = symb_name(q->src->symb, t);
    const uint8_t *const text = symb_text(q->src->symb, t);
    const uint8_t m = sugg_match(q->word, q->wn, text + 1, text[0]);

    if (!m || (k & BIND_TYPE && decl_type(q->src->decl, nm->rec)->alien))
        return;

    const SuggKind kind = nm->flags & NM_SUB ? SG_SUB : nm->flags & NM_FUNC ? SG_FN : SG_VAR;
    const Sugg s = sugg_named(q->src, t, text + 1, text[0], kind, m, 50);
    sugg_add(q->out, &s);
}

/* After an imported module's name and '.': its exports the word starts; false not after one. */
static bool sugg_exports(
    const SuggSrc *const src,
    Suggs *const out,
    const uint8_t *const word,
    const uint16_t wn
)
{
    const uint8_t *const b = sg.before;

    if (!sg.nbefore || b[sg.nbefore - 1] != '.')
        return false;

    const uint16_t e = sg.nbefore - 1;
    uint16_t s = e;

    while (s && is_name_char(b[s - 1])) /* a package's module: lib.gfx */
        s--;

    if (s == e || e - s > UINT8_MAX || !is_letter(b[s]))
        return false;

    const uint32_t name = symb_look_name(src->symb, src->module, b + s, (uint8_t)(e - s));

    if (!name || !(symb_name(src->symb, name)->flags & NM_MOD))
        return false;

    SuggExports q = {src, out, word, wn};

    bind_each_export(
        src->scop,
        src->symb,
        *symb_alias(src->symb, name),
        UINT8_MAX,
        sugg_export,
        &q
    );
    return true;
}

/* The word starts a statement: the line, ':', THEN or ELSE before it. */
static bool sugg_starts(void)
{
    uint16_t n = sg.nbefore;

    while (n && sg.before[n - 1] == ' ')
        n--;

    return !n || sg.before[n - 1] == ':' || sugg_endsin(n, "THEN") || sugg_endsin(n, "ELSE");
}

static void sugg_program(
    const SuggSrc *const src,
    const SuggAt *const w,
    Suggs *const out,
    const uint8_t *const word,
    const uint16_t n
)
{
    const Prog *const pg = src->prog;
    uint32_t scope = 0;
    uint32_t ord = 0;
    uint8_t prior = 0;
    bool intype = false;

    for (uint32_t rec = PROGBASE; prog_line(pg, rec)->len; rec += prog_line(pg, rec)->len, ord++) {
        const Line *const ln = prog_line(pg, rec);

        if (ln->flags & LF_ASIS || !sugg_ours(src, ln))
            continue;

        const uint8_t *const body = at(prog_addr(pg, rec + HDR));
        const uint32_t dist = ord > w->ord ? ord - w->ord : w->ord - ord;
        const uint8_t *name;
        const uint8_t kind = decl_proc(body, &name);

        if (kind == PK_SUB || kind == PK_FUNC) {
            scope = comp_nameof(name);

            if (sg.nprocs < NPROCS)
                sg.procs[sg.nprocs++] = scope;

            SuggName p;
            const uint8_t *past;

            if (sugg_name(name, &p, &past)) {
                p.len -= p.text[p.len - 1] == '(';
                p.text[p.len] = 0;
                const uint8_t m = sugg_match(word, n, p.text, p.len);

                if (m && !sg.jump && sugg_fits(src, p.text, p.len)) {
                    /* its own body's statement: the result set, no call */
                    const bool result = kind == PK_FUNC && scope == w->scope && sugg_starts();
                    const SuggKind k = kind == PK_SUB ? SG_SUB : result ? SG_VAR : SG_FN;
                    Sugg s = sugg_named(src, p.name, p.text, p.len, k, m, result ? 60 : 25);
                    s.rec = rec;
                    sugg_add(out, &s);
                }
            }
        }

        if (ln->num != NONUM && scope == w->scope &&
            (sg.jump || (sg.either && (!n || is_digit(*word))))) {
            uint8_t digits[8];
            const uint16_t len = sugg_decimal(ln->num, digits);
            const uint8_t m = sugg_match(word, n, digits, (uint16_t)len);
            const bool start =
                prior == TK_RET || prior == TK_END || prior == TK_STOP || prior == TK_GOTO;

            if (m || !n) {
                Sugg s = sugg_make(
                    digits,
                    (uint16_t)len,
                    SG_LABEL,
                    m ? m : 2,
                    (int16_t)((dist < 30 ? 30 - dist : 0) +
                              (sg.last == TK_GOSUB && start ? 100 : 0) + (sg.either ? 40 : 0))
                );
                s.rec = rec;
                s.need = sg.jump; /* the jump's */
                sugg_add(out, &s);
            }
        }

        intype = (intype || *body == TK_TYPE) && !(*body == TK_END && body[1] == TK_TYPE);

        if (!intype && !(*body == TK_END && body[1] == TK_TYPE)) /* a TYPE's fields: no variables */
            sugg_names(body, ln->len - HDR, scope, dist);
        if (kind == PK_END)
            scope = 0;

        const uint8_t *t = body;
        decl_tail(&t);
        prior = *t;
    }
}

/* The names seen from the word's procedure that match the word and fit; not a procedure's. */
static void sugg_vars(
    const SuggSrc *const src,
    const SuggAt *const w,
    Suggs *const out,
    const uint8_t *const word,
    const uint16_t n
)
{
    const SuggName *const open = sg.nopen && sg.last == TK_NEXT ? &sg.open[sg.nopen - 1] : nullptr;

    for (uint16_t i = 0; i < sg.nnames; i++) {
        const SuggName *const e = &sg.names[i];
        const uint8_t m = sugg_match(word, n, e->text, e->len);

        if (!m || !sugg_seen(src, e, w->scope))
            continue;

        bool proc = false; /* a procedure's name: listed as one */

        for (uint16_t j = 0; j < sg.nprocs; j++)
            proc |= sg.procs[j] == e->name;

        uint8_t probe[SUGG_LEN + 4];
        memcpy(probe, e->text, e->len);
        uint16_t pn = e->len;

        if (e->text[e->len - 1] == '(') { /* an element */
            probe[pn++] = '0';
            probe[pn++] = ')';
        }

        if (proc || !sugg_fits(src, probe, pn))
            continue;

        const uint16_t count = e->count < 10 ? e->count : 10;
        const int16_t near = (int16_t)(e->dist < 30 ? 30 - e->dist : 0);
        const bool forvar = open && open->name == e->name;
        const int16_t score = (int16_t)(3 * count + near + (forvar ? 200 : 0));
        Sugg s = sugg_named(src, e->name, e->text, e->len, SG_VAR, m, score);
        s.need = forvar;
        sugg_add(out, &s);
    }
}

/* ---- the suggestions */

/* The word before col in text: its start; false none fits (inside a word, past the line). */
static bool sugg_word(
    const uint8_t *const text,
    const uint16_t n,
    const uint16_t col,
    uint16_t *const start
)
{
    if (col > n || (col < n && is_word_char(text[col])))
        return false;

    uint16_t s = col;

    if (s >= 2 && sugg_suffix(text[s - 1]) && is_letter(text[s - 2])) /* a name's type: its own */
        s--;
    while (s && is_word_char(text[s - 1]))
        s--;
    if (s < col && is_digit(text[s]) && s >= 2 && text[s - 1] == '.' && is_digit(text[s - 2]))
        for (s--; s && is_digit(text[s - 1]); s--) /* a number's whole part */
            ;

    *start = s;
    return col - s <= SUGG_LEN;
}

/* ---- a text dialect's */

typedef struct {
    Suggs *out;
    const uint8_t *word; /* capitals */
    uint16_t wn;
} SuggText;

/* Dialect.suggest's: text, len bytes, a kind, offered when it fits the word; as written. */
static void sugg_text_one(
    void *const ctx,
    const char *const text,
    const uint32_t len,
    const DialSuggKind kind,
    const uint32_t rec
)
{
    const SuggText *const q = ctx;
    uint8_t up[SUGG_LEN];

    if (len > SUGG_LEN)
        return;

    for (uint32_t i = 0; i < len; i++)
        up[i] = upcase((uint8_t)text[i]);

    const uint8_t match = sugg_match(q->word, q->wn, up, (uint16_t)len);

    if (!match)
        return;

    const SuggKind k = kind == DSG_KW ? SG_KW : kind == DSG_FN ? SG_FN : SG_VAR;
    Sugg s = sugg_make((const uint8_t *)text, (uint16_t)len, k, match, kind == DSG_KW ? 0 : 10);
    s.asis = s.len;
    s.rec = kind == DSG_FN ? rec : 0;
    sugg_add(q->out, &s);
}

/* One thing alone goes on: it is needed. */
static void sugg_one(Suggs *const out)
{
    if (out->n == 1 && !out->list[0].fuzzy)
        out->list[0].need = true;
}

bool sugg_find(
    Session *const S,
    const uint8_t *const text,
    const uint16_t n,
    const uint16_t col,
    const uint32_t rec,
    Suggs *const out
)
{
    uint16_t start;

    if (!sugg_word(text, n, col, &start) || start >= TEXT_MAX)
        return false;
    if (!sg.verdict) {
        sg.verdict = pmem_resize(nullptr, NVERDICT * sizeof *sg.verdict);

        if (!sg.verdict)
            return false;

        sg.nbefore = UINT16_MAX; /* the verdicts made at the first line */
    }

    out->start = start;
    out->n = 0;
    uint8_t word[SUGG_LEN + 1];
    const uint16_t wn = col - start;

    for (uint16_t i = 0; i < wn; i++)
        word[i] = upcase(text[start + i]);

    word[wn] = 0;

    const Dialect *const d = dial_line(&S->prog, rec);

    if (d->text) {
        SuggText q = {out, word, wn};

        d->suggest(S, rec, (const char *)text, start, sugg_text_one, &q);
        sugg_one(out);
        return out->n > 0;
    }

    memcpy(sg.body, text, start); /* the line before the word: not in a string, a REM or DATA */
    sg.body[start] = 0;
    uint8_t *const end = lexr_line(&S->lexr, sg.body, S->comp.cbuf);
    const uint32_t blen = (uint32_t)(end - S->comp.cbuf - 1);
    memcpy(sg.body, S->comp.cbuf, blen + 1);
    const uint8_t *tail = sg.body;

    if (decl_tail(&tail))
        return false;

    const uint32_t module = modu_of(&S->modu, prog_line(&S->prog, rec)->file);
    const SuggSrc src = {
        &S->comp,
        &S->lexr,
        &S->prog,
        &S->scop,
        &S->decl,
        &S->symb,
        &S->modu,
        module,
    };

    sugg_tail(sg.body, blen);
    SuggAt w = {.rec = rec};

    sugg_where(&src, &w);
    sugg_events(sg.body, sg.body + blen);
    sugg_before(&src, text, start);

    if (sg.either) /* THEN 0: a line number, not an operand */
        sg.str_ok = sg.num_ok = false;
    if (sugg_fields(&src, out, word, wn)) /* r.f: the record's fields alone */
        return out->n > 0;
    if (sugg_exports(&src, out, word, wn)) { /* m.f: the module's exports alone */
        sugg_one(out);
        return out->n > 0;
    }

    if (sg.hash) { /* #n: the program's files */
        sugg_files(&src, out, word, wn);
        sugg_one(out);
        return out->n > 0;
    }

    const bool number =
        wn && is_digit(word[0]) && !sg.jump && !sg.either; /* what goes on after it */

    sg.nnames = 0;
    memset(sg.head, 0xFF, sizeof sg.head);
    sugg_names(sg.body, blen, w.scope, 0);

    if (!wn && sg.last == TK_NEXT && sg.nopen) { /* NEXT: the open FOR's variable alone */
        const SuggName *const v = &sg.open[sg.nopen - 1];
        Sugg s = sugg_named(&src, v->name, v->text, v->len, SG_VAR, 2, 0);
        s.need = true;
        sugg_add(out, &s);
        return true;
    }

    if (!sg.jump && !number) /* a jump's target: its line numbers alone */
        sugg_keys(&src, out, word, wn);

    sg.nprocs = 0;

    if (!number)
        sugg_program(&src, &w, out, word, wn);
    if (!sg.jump && !number)
        sugg_vars(&src, &w, out, word, wn);
    if (!sg.jump)
        sugg_follow(&src, out, text + start, word, wn);
    if (sg.as)
        sugg_types(&src, out, word, wn);

    uint8_t fn[SUGG_LEN + 1];

    if (sugg_callee(sugg_paren(), fn) == 3 && /* EOF(, LOF(, LOC(: a file */
        (!memcmp(fn, "EOF", 3) || !memcmp(fn, "LOF", 3) || !memcmp(fn, "LOC", 3)))
        sugg_files(&src, out, word, wn);

    sugg_one(out);
    return out->n > 0;
}

void sugg_used(const Sugg *const s)
{
    uint8_t i = 0;

    while (i < sg.nrecent && strcmp((const char *)sg.recent[i], (const char *)s->text))
        i++;

    if (i == NRECENT)
        i--;
    if (i == sg.nrecent)
        sg.nrecent++;

    memmove(sg.recent[1], sg.recent[0], i * sizeof sg.recent[0]);
    memcpy(sg.recent[0], s->text, s->len + 1u);
}

const Sugg *sugg_guess(const Suggs *const s, const uint16_t wn)
{
    if (!s->n)
        return nullptr;

    const Sugg *const e = &s->list[0];

    if (e->fuzzy || e->len <= wn || ((!wn || e->kind == SG_PUNCT) && !e->need))
        return nullptr;

    return e;
}
