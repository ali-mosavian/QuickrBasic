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

bool path_join(
    char *const out,
    const uint32_t cap,
    const char *const dir,
    uint32_t n,
    const char *const name
)
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

/* path's attributes; false: it is not there. */
static bool attributes(const char *const path, uint8_t *const attr)
{
    return pfil_getattr(path, attr) == PFIL_OK;
}

bool path_exists(const char *const path)
{
    uint8_t attr;
    return attributes(path, &attr) && !(attr & PFIL_ADIR);
}

bool path_isdir(const char *const path)
{
    uint8_t attr;
    return attributes(path, &attr) && attr & PFIL_ADIR;
}

/* The part at p: its length, to the next separator or the end. */
static uint32_t part(const char *const p)
{
    uint32_t n = 0;

    while (p[n] && !is_sep(p[n]))
        n++;

    return n;
}

void path_norm(char *const path)
{
    const uint32_t root = is_sep(path[0]) ? 1 : path[0] && path[1] == ':' ? 2 + is_sep(path[2]) : 0;
    uint32_t w = root, kept = 0; /* kept: the parts written that ".." can take back */

    for (uint32_t r = root; path[r];) {
        const uint32_t n = part(path + r), next = r + n + (path[r + n] != 0);

        if (n == 1 && path[r] == '.') {
            r = next;
            continue;
        }

        const bool up = n == 2 && path[r] == '.' && path[r + 1] == '.';

        if (up && (kept || root)) { /* the part before taken back; at the root, nothing */
            for (; kept && w > root && is_sep(path[w - 1]);)
                w--;
            for (; kept && w > root && !is_sep(path[w - 1]);)
                w--;

            kept -= kept != 0;
            r = next;
            continue;
        }

        memmove(path + w, path + r, next - r);
        w += next - r;
        kept += !up;
        r = next;
    }

    path[w] = 0;
}
