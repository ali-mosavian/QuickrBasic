/* The string routines of lib/include/string.h.
 *
 * # How it works
 * clang calls memcpy, memmove and memset in the build's convention (-mregparm=3), not llrm's
 * runtime's: they are here, renamed in the objects (Makefile) so as not to clash with the
 * runtime's. On the 386 memcpy, memmove, memset, memcmp, memchr and strlen are plat/dos32/string.asm's;
 * the C below is any other target's.
 */
#include <stdint.h>
#include <string.h>

#undef memcpy
#undef memmove
#undef memset

#if !(defined(__i386__) && defined(_WIN32))
void *memcpy(void *const dst, const void *const src, const size_t n)
{
    unsigned char *const d = dst;
    const unsigned char *const s = src;

    for (size_t i = 0; i < n; i++)
        d[i] = s[i];

    return dst;
}

void *memmove(void *const dst, const void *const src, const size_t n)
{
    unsigned char *const d = dst;
    const unsigned char *const s = src;

    if (d <= s || d >= s + n)
        return memcpy(dst, src, n);

    for (size_t i = n; i-- > 0;)
        d[i] = s[i];

    return dst;
}

void *memset(void *const dst, const int c, const size_t n)
{
    unsigned char *const d = dst;

    for (size_t i = 0; i < n; i++)
        d[i] = (unsigned char)c;

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
#endif


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
