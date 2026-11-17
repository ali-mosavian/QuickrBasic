/* The C library the core needs on wasm32: string.h's routines, and the runtime clang's
 * setjmp/longjmp lowering calls (as wasi-libc's).
 *
 * # How it works
 * A copy or a fill is one bulk-memory instruction. setjmp records the calling function's
 * invocation and a label; longjmp throws C_LONGJMP, which that function's code catches and
 * matches with __wasm_setjmp_test.
 */
#include <setjmp.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#undef memcpy
#undef memmove
#undef memset

void *memcpy(void *const dst, const void *const src, const size_t n)
{
    return __builtin_memcpy(dst, src, n);
}
void *memmove(void *const dst, const void *const src, const size_t n)
{
    return __builtin_memmove(dst, src, n);
}
void *memset(void *const dst, const int c, const size_t n) { return __builtin_memset(dst, c, n); }

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
            return (void *)(p + i);

    return nullptr;
}

size_t strlen(const char *const s)
{
    size_t n = 0;

    while (s[n])
        n++;

    return n;
}

int strncmp(const char *const a, const char *const b, const size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i] || !a[i])
            return (unsigned char)a[i] - (unsigned char)b[i];

    return 0;
}

int strcmp(const char *const a, const char *const b) { return strncmp(a, b, SIZE_MAX); }

char *strchr(const char *const s, const int c)
{
    for (const char *p = s;; p++) {
        if (*p == (char)c)
            return (char *)p;
        if (!*p)
            return nullptr;
    }
}

enum { C_LONGJMP = 1 }; /* the exception tag clang's lowering catches */

void __wasm_setjmp(void *const env, const uint32_t label, void *const invocation)
{
    jmp_buf *const jb = env;

    (*jb)->invocation = invocation;
    (*jb)->label = label;
}

uint32_t __wasm_setjmp_test(void *const env, void *const invocation)
{
    const jmp_buf *const jb = env;

    return (*jb)->invocation == invocation ? (*jb)->label : 0;
}

void __wasm_longjmp(void *const env, const int value)
{
    jmp_buf *const jb = env;

    (*jb)->env = env;
    (*jb)->value = value ? value : 1;
    __builtin_wasm_throw(C_LONGJMP, &(*jb)->env);
}

char *strrchr(const char *const s, const int c)
{
    const char *last = nullptr;

    for (const char *p = s;; p++) {
        if (*p == (char)c)
            last = p;
        if (!*p)
            return (char *)last;
    }
}

char *strncpy(char *const dst, const char *const src, const size_t n)
{
    size_t i = 0;

    for (; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = 0;

    return dst;
}

char *strcpy(char *const dst, const char *const src) { return memcpy(dst, src, strlen(src) + 1); }
char *strcat(char *const dst, const char *const src)
{
    strcpy(dst + strlen(dst), src);
    return dst;
}

static int lower(const int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

int strncasecmp(const char *const a, const char *const b, const size_t n)
{
    for (size_t i = 0; i < n; i++) {
        const int x = lower((unsigned char)a[i]), y = lower((unsigned char)b[i]);

        if (x != y || !x)
            return x - y;
    }

    return 0;
}

int strcasecmp(const char *const a, const char *const b) { return strncasecmp(a, b, SIZE_MAX); }

/* ---- snprintf: %s %c %d %u %x %ld %lu %lld %llu, a width and 0 or - ---- */

typedef struct {
    char *buf;
    size_t cap, n;
} Out;

static void put(Out *const o, const char c)
{
    if (o->n + 1 < o->cap)
        o->buf[o->n] = c;

    o->n++;
}

static void put_num(
    Out *const o,
    uint64_t v,
    const bool neg,
    const uint32_t base,
    uint32_t width,
    const char pad,
    const bool left
)
{
    char d[24];
    uint32_t n = 0;

    do {
        d[n++] = "0123456789abcdef"[v % base];
        v /= base;
    } while (v);

    if (neg)
        d[n++] = '-';

    for (; !left && width > n; width--)
        put(o, pad);
    while (n)
        put(o, d[--n]), width -= width > 0;
    for (; left && width; width--)
        put(o, ' ');
}

int vsnprintf(char *const buf, const size_t cap, const char *f, va_list ap)
{
    Out o = {buf, cap, 0};

    for (; *f; f++) {
        if (*f != '%') {
            put(&o, *f);
            continue;
        }

        const bool left = f[1] == '-';
        f += left;
        const char pad = f[1] == '0' ? '0' : ' ';
        uint32_t width = 0;

        while (f[1] >= '0' && f[1] <= '9')
            width = width * 10 + (uint32_t)(*++f - '0');

        uint32_t longs = 0;

        while (f[1] == 'l')
            longs++, f++;

        switch (*++f) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            const size_t n = strlen(s);

            for (size_t i = n; !left && i < width; i++)
                put(&o, ' ');
            while (*s)
                put(&o, *s++);
            for (size_t i = n; left && i < width; i++)
                put(&o, ' ');

            break;
        }
        case 'c':
            put(&o, (char)va_arg(ap, int));
            break;

        case 'd': {
            const int64_t v = longs > 1 ? va_arg(ap, long long)
                              : longs   ? va_arg(ap, long)
                                        : va_arg(ap, int);
            put_num(&o, v < 0 ? 0 - (uint64_t)v : (uint64_t)v, v < 0, 10, width, pad, left);
            break;
        }
        case 'u':
        case 'x': {
            const uint64_t v = longs > 1 ? va_arg(ap, unsigned long long)
                               : longs   ? va_arg(ap, unsigned long)
                                         : va_arg(ap, unsigned);
            put_num(&o, v, false, *f == 'x' ? 16 : 10, width, pad, left);
            break;
        }
        default:
            put(&o, *f);
        }
    }

    if (cap)
        buf[o.n < cap ? o.n : cap - 1] = 0;

    return (int)o.n;
}

int snprintf(char *const buf, const size_t cap, const char *const fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return n;
}
