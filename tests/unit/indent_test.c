/* The editor's auto indent (ui/indent.h): Enter's indent for the line and the next. */
#include "session.h"
#include "ui/indent.h"
#include "unit.h"

static const char *const *lines; /* the lines above */

static uint16_t get(void *const ctx, const uint32_t i, uint8_t *const dst)
{
    const uint16_t n = (uint16_t)strlen(lines[i]);
    memcpy(dst, lines[i], n);
    return n;
}

/* Enter at the end of text, typed below the n lines above: the line's indent and the next's. */
static bool enter(
    const char *const *above,
    const uint32_t n,
    const char *const text,
    const uint16_t self,
    const uint16_t next
)
{
    uint16_t s, x;
    lines = above;
    const IndtLines ls = {.d = &dialect_qbasic, .get = get};

    indt_enter(&ls, n, (const uint8_t *)text, (uint16_t)strlen(text), &s, &x);
    return s == self && x == next;
}

/* A block's start: the next line a step in; a plain line: as it is. */
TEST(indent_opens)
{
    CHECK(enter(nullptr, 0, "FOR i = 1 TO 3", 0, 4));
    CHECK(enter(nullptr, 0, "  x = 1", 2, 2));
    CHECK(enter(nullptr, 0, "IF a THEN", 0, 4));
    CHECK(enter(nullptr, 0, "IF a THEN PRINT 1: PRINT 2", 0, 0)); /* a line's IF */
    CHECK(enter(nullptr, 0, "IF a THEN ' block", 0, 4));
    CHECK(enter(nullptr, 0, "DO WHILE x < 3", 0, 4));
    CHECK(enter(nullptr, 0, "FOR i = 1 TO 3: NEXT", 0, 0));
    CHECK(enter(nullptr, 0, "SELECT CASE k", 0, 4));
    CHECK(enter(nullptr, 0, "10 WHILE x", 0, 4));
    CHECK(enter(nullptr, 0, "PRINT \"FOR\"", 0, 0));
    CHECK(enter(nullptr, 0, "REM FOR", 0, 0));
    CHECK(enter(nullptr, 0, "do_it: x = 1", 0, 0)); /* '_' in a name, ours: it was DO */
}

/* An end back at its start's column, across blocks of its kind between. */
TEST(indent_closes)
{
    static const char *const above[] = {
        "FOR i = 1 TO 3",
        "    FOR j = 1 TO 3",
        "        PRINT j",
        "    NEXT",
        "    PRINT i",
    };

    CHECK(enter(above, 5, "    NEXT", 0, 0));
    CHECK(enter(above, 3, "        NEXT j", 4, 4));
    CHECK(enter(above, 5, "    next i", 0, 0)); /* typed in small letters */
}

/* ELSE at its IF, its body a step in; CASE a step in from SELECT, its body two. */
TEST(indent_middles)
{
    static const char *const ifs[] = {"IF a THEN", "    PRINT 1"};

    CHECK(enter(ifs, 2, "    ELSE", 0, 4));
    CHECK(enter(ifs, 2, "    END IF", 0, 0));
    static const char *const sel[] = {"SELECT CASE k", "    CASE 1", "        PRINT 1"};

    CHECK(enter(sel, 1, "    CASE 1", 4, 8));
    CHECK(enter(sel, 3, "        CASE 2", 4, 8));
    CHECK(enter(sel, 3, "        END SELECT", 0, 0));
}
