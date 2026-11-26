/* What clang's code asks of a C library, in gfx's DOS slice, which imports nothing: copies and
 * fills here, the 64-bit division's helpers the runtime's lib/lldiv.c, built in beside.
 */
#include <stddef.h>
#include <stdint.h>

void *memcpy(void *const d, const void *const s, const size_t n)
{
    void *to = d;
    const void *from = s;
    size_t words = n >> 2, bytes = n & 3;

    __asm__ volatile("rep movsl" : "+D"(to), "+S"(from), "+c"(words) : : "memory");
    __asm__ volatile("rep movsb" : "+D"(to), "+S"(from), "+c"(bytes) : : "memory");
    return d;
}

void *memset(void *const d, const int v, const size_t n)
{
    void *to = d;
    size_t words = n >> 2, bytes = n & 3;
    const uint32_t fill = (uint8_t)v * 0x01010101u;

    __asm__ volatile("rep stosl" : "+D"(to), "+c"(words) : "a"(fill) : "memory");
    __asm__ volatile("rep stosb" : "+D"(to), "+c"(bytes) : "a"(fill) : "memory");
    return d;
}
