/* comp/blocks: the procedure a line is in, by a search of the procedures by first line. Quickr's
   are added after QBasic's, with lines before theirs, and placeholders of no line after: the
   search went past QBasic's art.bas glyph_row, its DIM n AS INTEGER compiled as the module's, and
   OPTION EXPLICIT said Variable not declared. */
#include "comp/blocks.h"
#include "unit.h"

[[noreturn]]
static void unraised(Fault *const f, const Failure *const e)
{
    (void)f;
    (void)e;
    __builtin_trap();
}

static Fault none = {.raise = unraised};

static void add(Blok *const b, const uint32_t first, const uint32_t last)
{
    Proc *const p = blok_add_proc(b, &none);
    p->first = first;
    p->last = last;
}

TEST(procof_finds_a_line_past_procedures_added_out_of_order)
{
    Blok b = {};

    blok_reset(&b, &none);

    for (uint32_t i = 1; i <= 4; i++) /* QBasic's, as the scan met them: 1 .. 4 */
        add(&b, i * 100, i * 100 + 50);

    add(&b, 20, 60); /* Quickr's: 5, 6 */
    add(&b, 600, 650);

    for (uint32_t i = 0; i < 12; i++) /* placeholders, no line */
        add(&b, 0, 0);

    for (uint32_t i = 1; i <= 4; i++)
        CHECK(blok_procof(&b, i * 100 + 10) == i);

    CHECK(blok_procof(&b, 30) == 5);
    CHECK(blok_procof(&b, 610) == 6);
    CHECK(blok_procof(&b, 70) == 0);
    CHECK(blok_procof(&b, 170) == 0);
    CHECK(blok_line_before(&b, 320) == 300);
    CHECK(blok_line_before(&b, 10) == PROGBASE);
    add(&b, 700, 750); /* added after a search: searched too */
    CHECK(blok_procof(&b, 710) == 19);
    arena_reset(&b.arena);
}
