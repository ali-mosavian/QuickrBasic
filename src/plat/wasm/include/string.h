/* The core's string.h (src/lib), and what the host's pfil.c and the runtime add. */
#ifndef WASM_STRING_H
#define WASM_STRING_H
#include_next <string.h>
char *strrchr(const char *s, int c);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
char *strcat(char *dst, const char *src);
int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, size_t n);
#endif
