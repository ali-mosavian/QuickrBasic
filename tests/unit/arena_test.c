/* util/arena: growth by blocks, reset's reuse, arenas apart. */
#include "session.h"
#include "unit.h"
#include "util/arena.h"

TEST(arena_grows_past_its_chunk_and_reuses_blocks_after_reset)
{
    Arena a;
    arena_init(&a, 64);
    uint8_t *p[10];
    for (uint32_t i = 0; i < 10; i++) {
        p[i] = arena_alloc(&a, 40);
        CHECK(p[i] && !p[i][0] && !p[i][39]);
        p[i][0] = 0xAA;
        for (uint32_t j = 0; j < i; j++)
            CHECK(p[j] != p[i]);
    }
    uint8_t *const big = arena_alloc(&a, 1000); /* past the chunk: a block of its own */
    CHECK(big);
    arena_reset(&a);
    uint8_t *const again = arena_alloc(&a, 40);
    CHECK(again == p[0] && !again[0]); /* the first block again, zeroed */
}

TEST(arenas_allocate_apart)
{
    Arena a, b;
    arena_init(&a, 128);
    arena_init(&b, 128);
    uint8_t *const x = arena_alloc(&a, 16), *const y = arena_alloc(&b, 16);
    CHECK(x && y && x != y);
    arena_reset(&a);
    CHECK(arena_alloc(&b, 16) != y);
}

TEST(arena_grow_extends_the_last_allocation_in_place_else_copies)
{
    Arena a;
    arena_init(&a, 256);
    uint8_t *p = arena_alloc(&a, 16);
    p[15] = 7;
    CHECK(arena_grow(&a, p, 16, 64) == p && p[15] == 7 && !p[63]);
    uint8_t *const q = arena_alloc(&a, 16); /* p no longer the last */
    uint8_t *const r = arena_grow(&a, p, 64, 128);
    CHECK(r && r != p && r != q && r[15] == 7 && !r[127]);
}
