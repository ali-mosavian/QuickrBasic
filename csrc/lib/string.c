/* The string routines of lib/include/string.h.
 *
 * # How it works
 * clang calls memcpy, memmove and memset in the build's convention (-mregparm=3), not llrm's
 * runtime's: they are here, renamed in the objects (Makefile) so as not to clash with the
 * runtime's. A copy or a fill is the CPU's string instructions, a dword a step.
 */
#include <stdint.h>
#include <string.h>

#undef memcpy
#undef memmove
#undef memset

/* rep movsd, then rep movsb for the rest: the CPU's own block copy, a dword a step. */
void *memcpy(void *const dst, const void *const src, const size_t n)
{
    void *d = dst;
    const void *s = src;
    size_t words = n / 4;

    __asm__ volatile("rep movsl\n\tmovl %3, %%ecx\n\trep movsb"
                     : "+D"(d), "+S"(s), "+c"(words)
                     : "r"(n % 4)
                     : "memory");
    return dst;
}

/* A dword at any address. */
typedef uint32_t __attribute__((aligned(1), may_alias)) Word;

/* Forward unless dst overlaps src's end; then from the end, a dword a step, in C: no direction
   flag set where an interrupt could find it. */
void *memmove(void *const dst, const void *const src, const size_t n)
{
    unsigned char *const d = dst;
    const unsigned char *const s = src;

    if (d <= s || d >= s + n)
        return memcpy(dst, src, n);

    const size_t words = n / sizeof(Word);

    for (size_t i = n; i-- > words * sizeof(Word);)
        d[i] = s[i];
    for (size_t i = words; i-- > 0;)
        ((Word *)d)[i] = ((const Word *)s)[i];

    return dst;
}

void *memset(void *const dst, const int c, const size_t n)
{
    void *d = dst;
    size_t words = n / 4;

    __asm__ volatile("rep stosl\n\tmovl %3, %%ecx\n\trep stosb"
                     : "+D"(d), "+c"(words)
                     : "a"((unsigned char)c * 0x01010101u), "r"(n % 4)
                     : "memory");
    return dst;
}

int memcmp(const void *const a, const void *const b, const size_t n)
{
    const unsigned char *const p = a, *const q = b;

    for (size_t i = 0; i < n; i++)
        if (p[i] != q[i])
            return p[i] - q[i];

    return 0;
}

void *memchr(const void *const s, const int c, const size_t n)
{
    const unsigned char *const p = s;

    for (size_t i = 0; i < n; i++)
        if (p[i] == (unsigned char)c)
            return (void *)&p[i];

    return nullptr;
}

size_t strlen(const char *const s)
{
    size_t n = 0;

    for (; s[n]; n++)
        ;

    return n;
}

int strcmp(const char *const a, const char *const b)
{
    size_t i = 0;

    for (; a[i] && a[i] == b[i]; i++)
        ;

    return (unsigned char)a[i] - (unsigned char)b[i];
}

int strncmp(const char *const a, const char *const b, const size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i] || !a[i])
            return (unsigned char)a[i] - (unsigned char)b[i];

    return 0;
}

char *strchr(const char *const s, const int c)
{
    for (size_t i = 0;; i++) {
        if (s[i] == (char)c)
            return (char *)&s[i];
        if (!s[i])
            return nullptr;
    }
}
