/* qr/qsort.c, a.sort's sorts, by their compares on keys whose order is known. */
#include <stdlib.h>
#include "qr/qsort.h"
#include "unit.h"

enum { N = 20000 };

typedef struct {
    QSort s;
    uint32_t *key, *tag;
    uint64_t compares, swaps;
} Keys;

static bool keys_less(QSort *const s, const uint32_t a, const uint32_t b)
{
    Keys *const k = (Keys *)s;
    k->compares++;
    return k->key[a] < k->key[b];
}

static void keys_swap(QSort *const s, const uint32_t a, const uint32_t b)
{
    Keys *const k = (Keys *)s;
    const uint32_t key = k->key[a], tag = k->tag[a];
    k->swaps++;
    k->key[a] = k->key[b];
    k->tag[a] = k->tag[b];
    k->key[b] = key;
    k->tag[b] = tag;
}

typedef enum { SORTED, REVERSED, EQUAL, FEW, PIPE, RANDOM, SHAPES } Shape;

/* n keys of shape, each tagged with where it starts. */
static Keys keys_made(const Shape shape, const uint32_t n)
{
    Keys k = {.s = {keys_less, keys_swap}};

    k.key = malloc(n * sizeof *k.key);
    k.tag = malloc(n * sizeof *k.tag);
    uint32_t x = 2463534242u;

    for (uint32_t i = 0; i < n; i++) {
        x ^= x << 13, x ^= x >> 17, x ^= x << 5;
        const uint32_t keys[SHAPES] = {i, n - i, 7, x % 4, i < n / 2 ? i : n - i, x};

        k.key[i] = keys[shape];
        k.tag[i] = i;
    }

    return k;
}

static void keys_free(Keys *const k)
{
    free(k->key);
    free(k->tag);
}

/* Are k's n keys in order, and when stable each run of equal ones in its start's? */
static bool keys_ordered(const Keys *const k, const uint32_t n, const bool stable)
{
    for (uint32_t i = 1; i < n; i++)
        if (k->key[i] < k->key[i - 1] ||
            (stable && k->key[i] == k->key[i - 1] && k->tag[i] < k->tag[i - 1]))
            return false;

    return true;
}

static uint64_t n_log_n(const uint32_t n)
{
    uint32_t log = 0;

    while (1u << log < n)
        log++;

    return (uint64_t)n * log;
}

/* An unstable merge put equal keys out of their order: records sorted by one field scrambled the
   order an earlier sort by another gave them. */
TEST(sort_merge_keeps_equal_keys_in_order)
{
    for (Shape shape = 0; shape < SHAPES; shape++) {
        Keys k = keys_made(shape, N);
        uint32_t *const buf = malloc(qsort_room(N) * sizeof *buf);
        qsort_merge(&k.s, N, buf);
        CHECK(keys_ordered(&k, N, true));
        CHECK(k.compares <= n_log_n(N));
        free(buf);
        keys_free(&k);
    }
}

/* A merge sort without runs took n log n compares, each a call into the lambda, on a list
   already in order. */
TEST(sort_merge_takes_n_minus_1_compares_on_runs)
{
    const Shape shapes[] = {SORTED, EQUAL};

    for (uint32_t i = 0; i < countof(shapes); i++) {
        Keys k = keys_made(shapes[i], N);
        uint32_t *const buf = malloc(qsort_room(N) * sizeof *buf);
        qsort_merge(&k.s, N, buf);
        CHECK(k.compares == N - 1);
        CHECK(k.swaps == 0);
        free(buf);
        keys_free(&k);
    }
}

/* A naive pivot or a two-way partition went quadratic, N * N / 2 compares, on sorted, reversed or
   equal keys. */
TEST(sort_quick_stays_n_log_n)
{
    for (Shape shape = 0; shape < SHAPES; shape++) {
        Keys k = keys_made(shape, N);
        qsort_quick(&k.s, N);
        CHECK(keys_ordered(&k, N, false));
        CHECK(k.compares <= 3 * n_log_n(N));
        keys_free(&k);
    }
}

/* Short lists, the sorts' edges: none, one, two, the insertion sort's and the ninther's. */
TEST(sort_short_lists)
{
    const uint32_t sizes[] = {0, 1, 2, 3, 12, 13, 39, 40, 41};

    for (uint32_t i = 0; i < countof(sizes); i++)
        for (Shape shape = 0; shape < SHAPES; shape++) {
            const uint32_t n = sizes[i];
            Keys k = keys_made(shape, n);
            uint32_t *const buf = malloc((qsort_room(n) + 1) * sizeof *buf);
            qsort_merge(&k.s, n, buf);
            CHECK(keys_ordered(&k, n, true));
            free(buf);
            keys_free(&k);

            k = keys_made(shape, n);
            qsort_quick(&k.s, n);
            CHECK(keys_ordered(&k, n, false));
            keys_free(&k);
        }
}
