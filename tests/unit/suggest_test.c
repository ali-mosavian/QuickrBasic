/* The editor's completion: what fits at the word, ranked (comp/suggest.h). */
#include "session.h"
#include "comp/suggest.h"
#include "unit.h"
#include <strings.h>

static Suggs out;

/* typed, the cursor at its end, on a new last line of the stored program: false none fit. */
static bool suggest(Session *const S, const char *const typed)
{
    const uint16_t n = (uint16_t)strlen(typed);
    return sugg_find(S, (const uint8_t *)typed, n, n, S->prog.end, &out);
}

/* text's place in the list in any casing, -1 not in it. */
static int place(const char *const text)
{
    for (uint16_t i = 0; i < out.n; i++)
        if (!strcasecmp((const char *)out.list[i].text, text))
            return i;

    return -1;
}

static const char *const program[] = {
    "10 DIM SHARED total AS LONG",
    "20 CONST limit = 9",
    "30 title$ = \"x\": prevval = 1: counterofitems = 2",
    "40 GOSUB 100",
    "50 RETURN",
    "60 PRINT 1",
    "100 PRINT title$",
    "110 SUB tally (amount)",
    "120 localonly = amount + total",
    "130 END SUB",
    "140 FUNCTION twice$ (s$)",
    "150 twice$ = s$ + s$",
    "160 END FUNCTION",
    nullptr,
};

/* A statement's start: statements, not functions or operators. */
TEST(suggest_a_statement)
{
    unit_store(S, program);
    CHECK(suggest(S, "pr"));
    CHECK(place("PRINT") == 0);
    CHECK(place("PRINT#") > 0);
    CHECK(suggest(S, "le"));
    CHECK(place("LET") >= 0 && place("LEN") < 0 && place("LEFT$") < 0);
}

/* After an operand only what goes on: FOR's TO, not TAN. */
TEST(suggest_what_goes_on)
{
    unit_store(S, program);
    CHECK(suggest(S, "for j=1 t"));
    CHECK(place("TO") == 0 && place("TAN") < 0);
}

/* A string wanted: string names and functions; no number, no LEN, no FN. */
TEST(suggest_a_string)
{
    unit_store(S, program);
    CHECK(suggest(S, "x$ = "));
    CHECK(place("TITLE$") >= 0 && place("CHR$") >= 0 && place("TWICE$") >= 0);
    CHECK(place("PREVVAL") < 0 && place("LEN") < 0 && place("FN") < 0);
    CHECK(out.list[place("TWICE$")].kind == SG_FN && place("TWICE$(") < 0);
}

/* A number wanted: no MID$; what goes on after the new name alone. */
TEST(suggest_a_number)
{
    unit_store(S, program);
    CHECK(suggest(S, "n = m"));
    CHECK(place("MID$") < 0 && out.list[0].kind == SG_PUNCT);
    CHECK(suggest(S, "n = l"));
    CHECK(place("LEN") >= 0 && place("LEFT$") < 0 && place("LIMIT") >= 0);
}

/* Nothing in a remark or a string. */
TEST(suggest_nothing_in_a_remark)
{
    unit_store(S, program);
    CHECK(!suggest(S, "rem pr"));
    CHECK(!suggest(S, "print \"pr"));
}

/* A name past ten letters was never listed. */
TEST(suggest_a_long_name)
{
    unit_store(S, program);
    CHECK(suggest(S, "print co"));
    CHECK(place("COUNTEROFITEMS") >= 0);
}

/* A SUB's local is its own; the module's SHARED and CONST are seen from it. */
TEST(suggest_by_scope)
{
    unit_store(S, program);
    CHECK(suggest(S, "print l"));
    CHECK(place("LOCALONLY") < 0 && place("LIMIT") >= 0);

    const uint16_t n = 8;
    uint32_t rec = PROGBASE; /* in tally: line 120's record */

    for (uint32_t i = 0; i < 8; i++)
        rec += prog_line(&S->prog, rec)->len;

    CHECK(sugg_find(S, (const uint8_t *)"print l ", n, n - 1, rec, &out));
    CHECK(place("LOCALONLY") >= 0 && place("LIMIT") >= 0);
    CHECK(sugg_find(S, (const uint8_t *)"print t ", n, n - 1, rec, &out));
    CHECK(place("TOTAL") >= 0 && place("TALLY") >= 0);
}

/* The word's letters in order find a name, after the names it starts. */
TEST(suggest_fuzzy)
{
    unit_store(S, program);
    CHECK(suggest(S, "print pv"));
    CHECK(place("PREVVAL") >= 0 && out.list[place("PREVVAL")].fuzzy);
}

/* GOSUB: a line after a RETURN first. */
TEST(suggest_a_gosub_target)
{
    unit_store(S, program);
    CHECK(suggest(S, "gosub "));
    CHECK(place("60") == 0 && place("REM") < 0 && place("PREVVAL") < 0);
    CHECK(suggest(S, "print 1")); /* not a line number: what goes on after it */
    CHECK(place("60") < 0 && place("1;") >= 0);
}

/* NEXT: the open FOR's variable first. */
TEST(suggest_the_open_for)
{
    unit_store(S, program);
    CHECK(suggest(S, "for prevval = 1 to 2: next p"));
    CHECK(place("PREVVAL") == 0);
    CHECK(suggest(S, "for prevval = 1 to 2: next "));
    CHECK(out.n == 1 && place("PREVVAL") == 0);
}

/* A function's name typed whole: its '('; an open one: the ')' that closes it, or the ',' to its
   next argument. */
TEST(suggest_punctuation)
{
    unit_store(S, program);
    CHECK(suggest(S, "x = len"));
    CHECK(place("LEN(") == 0);
    CHECK(suggest(S, "print left$("));
    CHECK(place(")") < 0);
    CHECK(suggest(S, "print left$(title$"));
    CHECK(place("TITLE$,") >= 0 && place("TITLE$)") < 0);
    CHECK(suggest(S, "print prevval"));
    CHECK(place("PREVVAL;") >= 0);
    CHECK(suggest(S, "pr"));
    CHECK(place("PR=") < 0);
    CHECK(suggest(S, "print left$(title$, 3 "));
    CHECK(place(")") == 0);
    CHECK(suggest(S, "print (1 + 2 "));
    CHECK(place(")") == 0 && place(",") < 0);
}

/* FOR's variable: its '='; its start: TO, after a space, when the start is whole. */
TEST(suggest_what_follows)
{
    unit_store(S, program);
    CHECK(suggest(S, "for i"));
    CHECK(place("I=") == 0 && out.list[0].score == SUGG_NEED); /* nothing else lets it go on */
    CHECK(suggest(S, "n = m"));
    CHECK(out.list[0].score < SUGG_NEED); /* the statement whole: nothing it needs */
    CHECK(suggest(S, "for i = 1"));
    CHECK(place("1 TO") == 0);
    CHECK(suggest(S, "for i = limit"));
    CHECK(place("LIMIT TO") == 0);
    CHECK(suggest(S, "for i = 1 to 9 "));
    CHECK(place("STEP") >= 0 && place("TO") < 0);
    CHECK(suggest(S, "if prevval "));
    CHECK(place("THEN") >= 0 && place("AND") >= 0);
}

/* One taken lately ranks first. */
TEST(suggest_what_was_taken)
{
    unit_store(S, program);
    CHECK(suggest(S, "pr"));
    const int i = place("PRINT#");
    CHECK(i > 0);
    sugg_used(&out.list[i]);
    CHECK(suggest(S, "pr"));
    CHECK(place("PRINT#") == 0);
}
/* After AS a type alone: the line check once took any word there, and INP came first. */
TEST(suggest_a_type)
{
    unit_store(S, program);
    CHECK(suggest(S, "dim x as in"));
    CHECK(out.n == 1 && place("INTEGER") == 0);
    CHECK(suggest(S, "function sum% (a as in"));
    CHECK(out.n == 1 && place("INTEGER") == 0);
    CHECK(!suggest(S, "function sum% (a as inp") || !out.n);
}

/* A new FUNCTION's parameter: ')', ',' or AS after it; the line check once skipped the line,
   so anything fit. */
TEST(suggest_a_parameter)
{
    unit_store(S, program);
    CHECK(suggest(S, "function sum% (a"));
    CHECK(place("A)") >= 0 && place("A AS") >= 0 && place("A=") < 0 && place("A THEN") < 0);
}

/* A name some other starts: what goes on after it not needed, it may be the other's start. */
TEST(suggest_a_start_not_needed)
{
    static const char *const two[] = {"10 i = 1: index = 2", nullptr};

    unit_store(S, two);
    CHECK(suggest(S, "for i"));
    CHECK(place("INDEX") >= 0 && place("I=") >= 0 && !out.list[place("I=")].need);
    CHECK(suggest(S, "for index"));
    CHECK(out.list[place("INDEX=")].need);
}

/* A FUNCTION's name starting a statement in its body sets the result: no '(' after it. */
TEST(suggest_a_result)
{
    unit_store(S, program);
    uint32_t rec = PROGBASE; /* in twice$: line 150's record */

    for (uint32_t i = 0; i < 11; i++)
        rec += prog_line(&S->prog, rec)->len;

    CHECK(sugg_find(S, (const uint8_t *)"tw", 2, 2, rec, &out));
    CHECK(out.list[place("TWICE$")].kind == SG_VAR);
    CHECK(sugg_find(S, (const uint8_t *)"x$ = tw", 7, 7, rec, &out));
    CHECK(out.list[place("TWICE$")].kind == SG_FN);
}

/* Names offered as first written: all were offered in capitals, TOTAL for Total, A for a TYPE's
   field a. */
TEST(suggest_names_as_written)
{
    static const char *const lines[] = {
        "10 TYPE pair",
        "20 a AS INTEGER",
        "30 END TYPE",
        "40 DIM x(10) AS pair",
        "50 Total = 1",
        "60 SUB Tally",
        "70 END SUB",
        nullptr,
    };

    unit_store(S, lines);
    CHECK(suggest(S, "print TOT") && !strcmp((const char *)out.list[place("Total")].text, "Total"));
    CHECK(suggest(S, "print x(0).") && !strcmp((const char *)out.list[0].text, "a"));
    CHECK(suggest(S, "dim y as PA") && !strcmp((const char *)out.list[0].text, "pair"));
    CHECK(suggest(S, "TAL") && !strcmp((const char *)out.list[place("Tally")].text, "Tally"));
}
