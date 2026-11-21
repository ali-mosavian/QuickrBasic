/* Native code on the Mac and Linux: the slice is a shared library, dlopen's.
 *
 * # How it works
 * dlopen takes a file. On Linux the slice is written to a memfd and opened through /proc, so no
 * file is left. The Mac has no memfd: the slice is written once to the user's cache, named by its
 * bytes' hash, so a rebuilt extension is a new file and an unchanged one is not written again.
 * Linux falls back to the cache when it has no memfd.
 */
#ifdef __linux__
#define _GNU_SOURCE /* memfd_create */
#include <sys/mman.h>
#endif
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "plat/pext.h"

#if defined(__APPLE__)
const char pext_tag[] = "macos";
#elif defined(__linux__) && defined(__x86_64__)
const char pext_tag[] = "linux-x86_64";
#else
const char pext_tag[] = "";
#endif

enum { PATH_CAP = 1024 };

#ifdef __APPLE__
#define SUFFIX ".dylib"
#else
#define SUFFIX ".so"
#endif

static bool put(const int fd, const uint8_t *p, uint32_t n)
{
    while (n) {
        const ssize_t w = write(fd, p, n);

        if (w <= 0)
            return false;

        p += w;
        n -= (uint32_t)w;
    }

    return true;
}

/* The cache's folder to out, made: false, no home. */
static bool cache_dir(char out[static PATH_CAP])
{
    const char *const home = getenv("HOME");
#ifdef __APPLE__
    const char *const parts[] = {"/Library", "/Library/Caches", "/Library/Caches/QuickrBasic"};
#else

    const char *const xdg = getenv("XDG_CACHE_HOME");

    if (xdg && *xdg) {
        snprintf(out, PATH_CAP, "%s/quickrbasic", xdg);
        mkdir(xdg, 0700);
        return mkdir(out, 0700) == 0 || access(out, W_OK) == 0;
    }

    const char *const parts[] = {"/.cache", "/.cache/quickrbasic"};
#endif

    if (!home || !*home)
        return false;

    for (size_t i = 0; i < sizeof parts / sizeof *parts; i++) {
        snprintf(out, PATH_CAP, "%s%s", home, parts[i]);
        mkdir(out, 0700);
    }

    return access(out, W_OK) == 0;
}

/* FNV-1a's 64 bits: the cached file's name. */
static uint64_t hash(const uint8_t *const p, const uint32_t n)
{
    uint64_t h = 0xcbf29ce484222325u;

    for (uint32_t i = 0; i < n; i++)
        h = (h ^ p[i]) * 0x100000001b3u;

    return h;
}

/* The slice in the cache: its file's path to out, written when it is not there. */
static bool cached(const uint8_t *const slice, const uint32_t bytes, char out[static PATH_CAP])
{
    char dir[PATH_CAP], tmp[PATH_CAP];

    if (!cache_dir(dir))
        return false;

    snprintf(out, PATH_CAP, "%s/%016llx%s", dir, (unsigned long long)hash(slice, bytes), SUFFIX);

    if (access(out, R_OK) == 0)
        return true;

    snprintf(tmp, sizeof tmp, "%s.%ld", out, (long)getpid());
    const int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0700);

    if (fd < 0)
        return false;

    const bool ok = put(fd, slice, bytes);
    close(fd);

    if (ok && rename(tmp, out) == 0) /* another run's at once: the same bytes */
        return true;

    unlink(tmp);
    return false;
}

QxInit *pext_load(const void *const slice, const uint32_t bytes, const char *const name)
{
    char path[PATH_CAP], entry[128];
    void *lib = nullptr;

    if (!*pext_tag || snprintf(entry, sizeof entry, "qx_init_%s", name) >= (int)sizeof entry)
        return nullptr;
#ifdef __linux__

    const int fd = memfd_create(name, MFD_CLOEXEC);

    if (fd >= 0) {
        snprintf(path, sizeof path, "/proc/self/fd/%d", fd);

        if (put(fd, slice, bytes))
            lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);

        close(fd);
    }
#endif

    if (!lib && cached(slice, bytes, path))
        lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);

    return lib ? (QxInit *)dlsym(lib, entry) : nullptr;
}
