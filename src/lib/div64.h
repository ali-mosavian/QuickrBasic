/* 64-bit division without the target's: lib/crt.c's helpers'. How it works: div64.c. */
#ifndef LIB_DIV64_H
#define LIB_DIV64_H
#include <stdint.h>

/* n / d, *rem n % d. d 0: the 32-bit division's fault. */
uint64_t div64_u(uint64_t n, uint64_t d, uint64_t *rem);
/* n / d truncated, *rem n % d with n's sign. d 0: the 32-bit division's fault. */
int64_t div64_s(int64_t n, int64_t d, int64_t *rem);
#endif
