/* The harness itself. */
#include "unit.h"

static void none(Session *const S) {}

/* The tests were kept in 256 places: past them a TEST was dropped, never run, and the count of
   those run did not show it. */
TEST(unit_keeps_every_test)
{
    static UnitTest t[300];
    UnitList l = {};

    l.end = &l.first;

    for (uint32_t i = 0; i < countof(t); i++) {
        t[i] = (UnitTest){"t", none, nullptr};

        unit_add(&l, &t[i]);
    }

    uint32_t n = 0;

    for (const UnitTest *p = l.first; p; p = p->next)
        CHECK(p == &t[n++]);

    CHECK(n == countof(t));
}
