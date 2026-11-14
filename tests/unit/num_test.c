/* Numbers' text: PRINT's spelling and VAL's reading (num/dubl). */
#include "num/double.h"
#include "unit.h"

static uint32_t fmt(const double v, uint8_t out[static DUBL_TXT]) { return dubl_fmt(v, out); }

TEST(print_spells_doubles_as_qbasic)
{
    uint8_t t[DUBL_TXT];
    uint32_t n = fmt(1.0 / 3, t);
    CHECK_TEXT(t, n, " .3333333333333333");
    n = fmt(-2.5, t);
    CHECK_TEXT(t, n, "-2.5");
    n = fmt(1e16, t);
    CHECK_TEXT(t, n, " 1D+16");
    n = fmt(0, t);
    CHECK_TEXT(t, n, " 0");
}

TEST(val_reads_kind_from_spelling)
{
    const uint8_t *p = (const uint8_t *)"1.5D3";
    uint8_t kind;
    CHECK(dubl_in(S, &p, &kind) == 1500 && kind == DK_DBL && !*p);
    p = (const uint8_t *)"&H10 ";
    CHECK(dubl_in(S, &p, nullptr) == 16);
    p = (const uint8_t *)"12345678";
    CHECK(dubl_in(S, &p, &kind) == 12345678 && kind == DK_DBL);
    p = (const uint8_t *)"2!";
    CHECK(dubl_in(S, &p, &kind) == 2 && kind == DK_SNGL);
}

TEST(print_rounds_once_at_64_bits)
{
    uint8_t t[DUBL_TXT];
    const uint32_t n = fmt(0.1 + 0.2, t); /* 0.30000000000000004: 16 digits round to .3 */
    CHECK_TEXT(t, n, " .3");
}
