/* FNV-1a: a hash of bytes, for telling texts apart. */
#ifndef UTIL_HASH_H
#define UTIL_HASH_H
#include <stdint.h>

enum : uint32_t { FNV_SEED = 2166136261u };

/* h after byte b. */
static inline uint32_t fnv_byte(const uint32_t h, const uint8_t b) { return (h ^ b) * 16777619u; }

/* h after the n bytes at s. */
static inline uint32_t fnv(uint32_t h, const uint8_t *const s, const uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        h = fnv_byte(h, s[i]);

    return h;
}
#endif
