/* The C library's string routines the core uses, for targets without one (dos32). */
#ifndef STRING_H
#define STRING_H
#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
void *memchr(const void *s, int c, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strchr(const char *s, int c);

/* -ffreestanding turns the builtins off: the fixed-size copies stay inline. */
#define memcpy(d, s, n) __builtin_memcpy(d, s, n)
#define memmove(d, s, n) __builtin_memmove(d, s, n)
#define memset(d, c, n) __builtin_memset(d, c, n)
#endif
