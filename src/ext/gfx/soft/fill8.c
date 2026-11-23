/* 8 bits a pixel's fillers (fill.h): an index a pixel, 0 the key. */
#include "mem.h"
#include "soft/fill.h"

static void span(uint8_t *const d, const int32_t n, const uint32_t colour)
{
    mem_fill(d, (uint8_t)colour, (size_t)n);
}

static void keyed(uint8_t *const d, const uint8_t *const s, const int32_t n)
{
    for (int32_t i = 0; i < n; i++)
        if (s[i])
            d[i] = s[i];
}

static void keyed_flipped(uint8_t *const d, const uint8_t *const s, const int32_t n)
{
    for (int32_t i = 0; i < n; i++)
        if (s[-i])
            d[i] = s[-i];
}

static void opaque(uint8_t *const d, const uint8_t *const s, const int32_t n)
{
    mem_copy(d, s, (size_t)n);
}

static void opaque_flipped(uint8_t *const d, const uint8_t *const s, const int32_t n)
{
    for (int32_t i = 0; i < n; i++)
        d[i] = s[-i];
}

const Fill fill8 = {
    .span = span,
    .row = {
        [0] = keyed,
        [ROW_FLIPX] = keyed_flipped,
        [ROW_OPAQUE] = opaque,
        [ROW_OPAQUE | ROW_FLIPX] = opaque_flipped,
    },
};
