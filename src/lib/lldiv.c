/* MSVC's helpers, which clang's i386-pc-win32 code calls for a 64-bit division or remainder:
 * __alldiv, __allrem, __aulldiv and __aullrem.
 *
 * # How it works
 * Undecorated stdcall, so each pops its divisor; the dividend comes as the build passes a first
 * argument, in EDX:EAX under the runtime's -mregparm=3, and the result goes back there.
 * lib/div64.c computes them. gfx's DOS slice, which links no C library, builds this in too.
 */
#include "lib/div64.h"

#define HELPER(type, name)                                                                         \
    [[gnu::stdcall]]                                                                               \
    type name(type n, type d) __asm__("__" #name)

HELPER(int64_t, alldiv);
HELPER(int64_t, allrem);
HELPER(uint64_t, aulldiv);
HELPER(uint64_t, aullrem);

int64_t alldiv(const int64_t n, const int64_t d)
{
    int64_t r;
    return div64_s(n, d, &r);
}

int64_t allrem(const int64_t n, const int64_t d)
{
    int64_t r;
    div64_s(n, d, &r);
    return r;
}

uint64_t aulldiv(const uint64_t n, const uint64_t d)
{
    uint64_t r;
    return div64_u(n, d, &r);
}

uint64_t aullrem(const uint64_t n, const uint64_t d)
{
    uint64_t r;
    div64_u(n, d, &r);
    return r;
}
