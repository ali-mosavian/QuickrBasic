/* Numbers' text: PRINT's spelling and VAL's reading (num/dubl). */
#include "num/double.h"
#include "num/fpmath.h"
#include "session.h"
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

/* A fault none of these raise. */
[[noreturn]]
static void unraised(Fault *const f, const Failure *const e)
{
    (void)f;
    (void)e;
    __builtin_trap();
}

static Fault none = {.raise = unraised};

TEST(val_reads_kind_from_spelling)
{
    const uint8_t *p = (const uint8_t *)"1.5D3";
    uint8_t kind;
    CHECK(dubl_in(&none, &p, &kind) == 1500 && kind == DK_DBL && !*p);
    p = (const uint8_t *)"&H10 ";
    CHECK(dubl_in(&none, &p, nullptr) == 16);
    p = (const uint8_t *)"12345678";
    CHECK(dubl_in(&none, &p, &kind) == 12345678 && kind == DK_DBL);
    p = (const uint8_t *)"2!";
    CHECK(dubl_in(&none, &p, &kind) == 2 && kind == DK_SNGL);
}

TEST(print_rounds_once_at_64_bits)
{
    uint8_t t[DUBL_TXT];
    const uint32_t n = fmt(0.1 + 0.2, t); /* 0.30000000000000004: 16 digits round to .3 */
    CHECK_TEXT(t, n, " .3");
}

/* A whole SINGLE to an integer by its bits, as the FPU's fistp gives it: every whole number to
   2^24 and past, each sign, and the halves and fractions about them, which take the fistp. */
TEST(conversions_of_whole_singles_match_the_fpus)
{
    for (int32_t i = -(1 << 24) - 8; i <= (1 << 24) + 8; i++) {
        const float f[] = {(float)i, (float)i + 0.5f, (float)i - 0.25f};

        for (uint32_t k = 0; k < 3; k++)
            CHECK(fmth_rint32(sngl_bits(f[k])) == pfpu_rint32(f[k]));
    }

    CHECK(fmth_rint32(sngl_bits(-0.0f)) == 0 && fmth_rint32(sngl_bits(3e9f)) == INT32_MIN);
}

/* A SINGLE or a DOUBLE to an integer: to the nearest, a half to even (the FPU's fistp); past the
   range, Illegal function call. A subscript too. */
TEST(conversions_round_half_to_even)
{
    static const char *const lines[] = {
        "10 DIM a%(4): a%(2) = 7: x = 1.5: y# = 2.5",
        "20 PRINT CINT(2.5); CINT(3.5); CINT(-2.5); CINT(-3.5); CLNG(y#); CLNG(-y#); a%(x)",
        "30 PRINT CLNG(-2147483648#); CLNG(-2147483648!)",
        "40 PRINT CLNG(2147483648!)",
        nullptr,
    };

    CHECK(unit_run(S, lines) == E_IQ);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 2  4 -2 -4  2 -2  7 \r-2147483648 -2147483648 \r");
}
