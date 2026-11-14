/* Completion's corpus: what a line typed so far is offered, simple and complex, against what a
   BASIC programmer would expect there. Each case: the line; the first offered ("" none fit);
   the rest shown dim past the cursor ("" none); '|'-separated texts offered, and not offered.
   nullptr: not checked. at: the line number being typed over, 0 a new last line. */
#include <stdio.h>
#include "session.h"
#include "comp/suggest.h"
#include "unit.h"

static const char *const program[] = {
    "10 TYPE pair",
    "20 x AS INTEGER",
    "30 y AS INTEGER",
    "40 END TYPE",
    "50 DIM SHARED total AS LONG, names$(10)",
    "60 CONST limit = 9",
    "70 title$ = \"x\": counter = 1",
    "80 GOSUB 200",
    "90 END",
    "200 PRINT title$",
    "210 RETURN",
    "220 SUB tally (amount)",
    "230 total = total + amount",
    "240 END SUB",
    "250 FUNCTION twice$ (s$)",
    "260 twice$ = s$ + s$",
    "270 END FUNCTION",
    "280 FUNCTION sum% (a AS INTEGER, b AS INTEGER)",
    "290 sum% = a + b",
    "300 END FUNCTION",
    nullptr,
};

typedef struct {
    const char *typed, *first, *guess, *has, *hasnt;
    uint16_t at;
} Case;

static const Case cases[] = {
    /* a statement's start */
    {"pr", "PRINT", "INT", "PRINT#", "LEN|COUNTER", 0},
    {"go", "GOTO", "TO", "GOSUB", "", 0},
    {"di", "DIM", "M", "", "", 0},
    {"wh", "WHILE", "ILE", "", "", 0},
    {"ta", nullptr, nullptr, "TALLY|TOTAL", "", 0},
    {"counter", nullptr, nullptr, "COUNTER=", "COUNTER THEN", 0},
    {"call ta", "TALLY", "LLY", "", "TOTAL", 0},
    /* an operand: its type's functions and names */
    {"print le", nullptr, nullptr, "LEN|LEFT$", "LET", 0},
    {"x = le", nullptr, nullptr, "LEN", "LEFT$|LET", 0},
    {"x$ = le", "LEFT$", "FT$", "", "LEN|LET", 0},
    {"x$ = mi", "MID$", "D$", "", "", 0},
    {"x = sq", "SQR", "R", "", "", 0},
    {"x = ti", "TIMER", "MER", "", "TITLE$|TIME$", 0},
    {"x$ = ti", nullptr, nullptr, "TITLE$|TIME$", "TIMER", 0},
    {"x$ = tw", "TWICE$", "ICE$", "", "", 0},
    {"x = tw", nullptr, nullptr, "", "TWICE$", 0},
    {"print ttl", nullptr, "", "TITLE$", "", 0},
    {"print 1", nullptr, nullptr, "", "200", 0},
    /* FOR, NEXT */
    {"for i", "I=", "=", "", "I THEN|I TO", 0},
    {"for i = 1", "1 TO", " TO", "", "", 0},
    {"for i = lim", "LIMIT", "IT", "", "", 0},
    {"for i = limit", "LIMIT TO", " TO", "", "", 0},
    {"for i = 1 to 10", nullptr, "", "10 STEP", "", 0},
    {"for i = 1 to 10 st", "STEP", "EP", "", "", 0},
    {"for i = 1 to 2: next ", "I", "I", "", "", 0},
    {"for i = 1 to 2: next i", nullptr, "", "", "", 0},
    /* IF */
    {"if counter", "COUNTER THEN", " THEN", "", "", 0},
    {"if counter > 1", "1 THEN", " THEN", "", "", 0},
    {"if counter > 1 then pr", "PRINT", "INT", "", "", 0},
    {"if counter > 1 then ", nullptr, "", "200|PRINT", "", 0},
    {"if counter > 1 then print 1 el", "ELSE", "SE", "", "", 0},
    /* jumps */
    {"gosub ", "200", "200", "210", "PRINT|230", 0},
    {"goto 2", nullptr, nullptr, "200|210", "230", 0},
    {"on counter goto 200, ", nullptr, nullptr, "210", "PRINT", 0},
    /* nothing in a remark, a string, DATA */
    {"rem pr", "", "", "", "", 0},
    {"print \"pr", "", "", "", "", 0},
    {"data 1, pr", "", "", "", "", 0},
    {"x = 1 ' pr", "", "", "", "", 0},
    /* types */
    {"dim a as in", "INTEGER", "TEGER", "", "INP|INPUT", 0},
    {"dim a as st", "STRING", "RING", "", "STR$", 0},
    {"dim p as pa", "PAIR", "IR", "", "", 0},
    {"dim a as ", nullptr, "", "INTEGER|LONG|STRING|PAIR", "PRINT|LEN", 0},
    {"function f% (a as in", "INTEGER", "TEGER", "", "", 0},
    {"sub foo (a", nullptr, nullptr, "A)|A AS|A,", "A=|A THEN", 0},
    {"sub foo (a as integer", nullptr, ")", "INTEGER)|INTEGER,", "INTEGER=", 0},
    /* procedures' scope */
    {"print am", "AMOUNT", "OUNT", "", "", 230},
    {"print am", nullptr, nullptr, "", "AMOUNT", 0},
    {"tw", "TWICE$", "ICE$", "", "", 260},
    /* parentheses */
    {"print (1 + 2", "2)", ")", "", "", 0},
    {"print left$(title$, 3", "3)", ")", "", "", 0},
    {"x = len(title$", "TITLE$)", ")", "", "", 0},
    {"print chr$(65", "65)", ")", "", "", 0},
    {"print mid$(", nullptr, "", "TITLE$", ")", 0},
    {"print (", nullptr, "", "", ")", 0},
    /* blocks, files, operators */
    {"select ca", "CASE", "SE", "", "", 0},
    {"do wh", "WHILE", "ILE", "", "", 0},
    {"do un", "UNTIL", "TIL", "", "", 0},
    {"open title$ for in", "INPUT", "PUT", "", "", 0},
    {"line in", "INPUT", "PUT", "", "", 0},
    {"print us", "USING", "ING", "", "", 0},
    {"x = 1 m", "MOD", "OD", "", "", 0},
    {"x = counter a", "AND", "ND", "", "", 0},
    {"print title$; ", nullptr, "", "", "", 0},
    /* deeper */
    {"print names", "NAMES$(", "$(", "", "", 0},
    {"x$ = title$ + ti", nullptr, nullptr, "TITLE$|TIME$", "TIMER", 0},
    {"if counter = 1 or co", "COUNTER", "UNTER", "", "", 0},
    {"x = abs(counter", "COUNTER)", ")", "", "", 0},
    {"x = abs(counter + li", "LIMIT", "MIT", "", "", 0},
    {"x = (counter + 1) * li", "LIMIT", "MIT", "", "", 0},
    {"print len(title$) + le", nullptr, nullptr, "LEN", "LEFT$", 0},
    {"x$ = twice$(ti",
     nullptr,
     nullptr,
     "TITLE$|TIME$",
     "",
     0}, /* its parameter's type: the link's */
    {"print tab(", nullptr, "", "", ")", 0},
    {"call tally(", nullptr, "", "COUNTER", ")", 0},
    {"print using \"##\"; co", "COUNTER", "UNTER", "", "", 0},
    {"print #1, ti", nullptr, nullptr, "TITLE$", "", 0},
    /* statements after statements */
    {"locate 1, 1: pr", "PRINT", "INT", "", "", 0},
    {"gosub 200: pr", "PRINT", "INT", "", "", 0},
    {"do while counter < 10: lo", "LOOP", "OP", "", "", 0},
    {"if inkey$ = \"\" then 2", nullptr, nullptr, "200|210", "", 0},
    /* jumps */
    {"goto ", "210", "210", "200", "PRINT", 0},
    {"on counter gosub ", nullptr, nullptr, "200|210", "PRINT", 0},
    {"restore ", nullptr, nullptr, "10|200", "PRINT", 0},
    /* in a procedure */
    {"exit s", "SUB", "UB", "", "", 230},
    {"end s", "SUB", "UB", "", "", 230},
    {"print amount + to", "TOTAL", "TAL", "", "", 230},
    {"sum% = a * ", nullptr, "", "A|B", "INTEGER|FUNCTION|PRINT|AS|AMOUNT", 290},
    /* input */
    {"input co", "COUNTER", "UNTER", "", "", 0},
    {"read co", "COUNTER", "UNTER", "", "CONT", 0},
    {"line input ti", nullptr, nullptr, "TITLE$", "TIMER", 0},
};

/* text's place in s, -1 not in it. */
static int corpus_place(const Suggs *const s, const char *const text, const size_t n)
{
    for (uint16_t i = 0; i < s->n; i++)
        if (strlen((const char *)s->list[i].text) == n && !memcmp(s->list[i].text, text, n))
            return i;

    return -1;
}

/* Each of the '|'-separated texts in or out of s. */
static bool corpus_each(const Suggs *const s, const char *list, const bool in)
{
    while (list && *list) {
        const char *const bar = strchr(list, '|');
        const size_t n = bar ? (size_t)(bar - list) : strlen(list);

        if ((corpus_place(s, list, n) >= 0) != in)
            return false;

        list = bar ? bar + 1 : nullptr;
    }

    return true;
}

/* The record of line num, else the program's end. */
static uint32_t corpus_rec(const Session *const S, const uint16_t num)
{
    for (uint32_t r = PROGBASE, len; num && (len = prog_line(&S->prog, r)->len); r += len)
        if (prog_line(&S->prog, r)->num == num)
            return r;

    return S->prog.end;
}

static bool corpus_case(Session *const S, const Case *const c)
{
    static Suggs out;
    const uint16_t n = (uint16_t)strlen(c->typed);
    const bool any = sugg_find(S, (const uint8_t *)c->typed, n, n, corpus_rec(S, c->at), &out);

    if (!any)
        out.n = 0;

    const uint16_t wn = out.n ? n - out.start : 0;
    const Sugg *const g = sugg_guess(&out, wn);
    const char *const guess = g ? (const char *)g->text + wn : "";
    const bool ok =
        (!c->first ||
         (*c->first ? out.n && !strcmp((const char *)out.list[0].text, c->first) : !out.n)) &&
        (!c->guess || !strcmp(guess, c->guess)) && corpus_each(&out, c->has, true) &&
        corpus_each(&out, c->hasnt, false);

    if (!ok) {
        printf("  \"%s\" at %u: guess \"%s\";", c->typed, c->at, guess);

        for (uint16_t i = 0; i < out.n && i < 8; i++)
            printf(" [%s %d%s]", out.list[i].text, out.list[i].score, out.list[i].need ? "!" : "");

        printf("\n");
    }

    return ok;
}

/* Every case as expected. */
TEST(suggest_corpus)
{
    unit_store(S, program);
    uint32_t bad = 0;

    for (uint32_t i = 0; i < countof(cases); i++)
        bad += !corpus_case(S, &cases[i]);

    CHECK(!bad);
}
