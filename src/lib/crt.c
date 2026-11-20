/* What clang's i386-pc-win32 code expects of a C runtime.
 *
 * # How it works
 * A function using floating point marks itself by referring to _fltused. A 64-bit division or
 * remainder calls MSVC's helpers __alldiv, __allrem, __aulldiv and __aullrem: undecorated stdcall,
 * so each pops its divisor; in the build's -mregparm=3 the dividend comes in EDX:EAX, the result
 * goes back there. lib/div64.c computes them.
 */
#include "lib/div64.h"

int _fltused;

#define HELPER(type, name) [[gnu::stdcall]] type name(type n, type d) __asm__("__" #name)

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
