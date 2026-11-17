/* snprintf's %s, %d, %u, %x, rename; no streams: fopen opens none (the runtime's PPM dump). */
#ifndef STDIO_H
#define STDIO_H
#include <stdarg.h>
#include <stddef.h>
typedef struct FILE FILE;
int snprintf(char *buf, size_t cap, const char *fmt, ...);
int vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap);
int rename(const char *from, const char *to);
static inline FILE *fopen(const char *const path, const char *const mode) { return (FILE *)0; }
static inline int fclose(FILE *const f) { return 0; }
static inline size_t fwrite(const void *const p, const size_t size, const size_t n, FILE *const f)
{
    return 0;
}
#define fprintf(f, ...) ((void)(f), 0)
#endif
