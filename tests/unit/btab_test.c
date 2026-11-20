/* util/btab: ids as block and place, entries that never move, reuse after a drop. */
#include "session.h"
#include "unit.h"
#include "util/btab.h"

TEST(btab_ids_split_into_block_and_place_and_entries_stay_put)
{
    Arena a;
    arena_init(&a, 256);
    BTab t;
    btab_init(&t, 12);
    uint8_t *const first = btab_add(&t, &a);
    uint8_t *mid = nullptr;
    first[0] = 7;

    for (uint32_t i = 1; i < 3 * BTAB_N; i++) {
        uint8_t *const e = btab_add(&t, &a);
        CHECK(e && !e[0] && !e[11]);
        e[0] = (uint8_t)i;

        if (i == BTAB_N + 5)
            mid = e;
    }

    CHECK(t.n == 3 * BTAB_N && t.nblocks == 3);
    CHECK(btab_at(&t, 0) == first && first[0] == 7); /* never moved */
    CHECK(btab_at(&t, BTAB_N + 5) == mid && mid == t.blocks[1] + 5 * 12);
    t.n = BTAB_N + 5; /* dropped: the room kept, the next add zeroed */
    uint8_t *const again = btab_add(&t, &a);
    CHECK(again == mid && !again[0] && t.nblocks == 3);
    arena_reset(&a);
}
