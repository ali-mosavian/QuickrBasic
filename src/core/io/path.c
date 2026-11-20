/* Paths. Interface: path.h. */
#include "io/path.h"
#include "plat/pfil.h"
#include <string.h>

static bool is_sep(const char c) { return c == '/' || c == '\\'; }

uint32_t path_dir(const char *const path)
{
    uint32_t n = 0;

    for (uint32_t i = 0; path[i]; i++)
        if (is_sep(path[i]) || path[i] == ':')
            n = i + 1;

    return n;
}

bool path_join(char *const out, const uint32_t cap, const char *const dir, uint32_t n,
               const char *const name)
{
    if (is_sep(name[0]) || (name[0] && name[1] == ':'))
        n = 0;

    const size_t len = strlen(name);

    if (n + len >= cap)
        return false;

    memmove(out + n, name, len + 1); /* out may be dir */
    memmove(out, dir, n);
    return true;
}

bool path_exists(const char *const path)
{
    uint8_t attr;
    return pfil_getattr(path, &attr) == PFIL_OK && !(attr & PFIL_ADIR);
}
