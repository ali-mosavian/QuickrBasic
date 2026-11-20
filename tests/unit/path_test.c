/* io/path.c: a path's "." and "dir/.." parts taken out, so two routes to one module are one. */
#include "io/path.h"
#include "unit.h"
#include <string.h>

static bool normed(const char *const path, const char *const want)
{
    char b[64];
    strcpy(b, path);
    path_norm(b);
    return !strcmp(b, want);
}

TEST(path_norm_takes_out_dots)
{
    (void)S;
    CHECK(normed("a/./b.bas", "a/b.bas"));
    CHECK(normed("a/b/../c.bas", "a/c.bas"));
    CHECK(normed("./x", "x"));
    CHECK(normed("lib/gfx/", "lib/gfx/"));
    CHECK(normed("a\\b\\..\\c", "a\\c"));
}

TEST(path_norm_keeps_what_it_cannot_take_back)
{
    (void)S;
    CHECK(normed("../x.bas", "../x.bas"));
    CHECK(normed("a/../../x", "../x"));
    CHECK(normed("/a/../b", "/b"));
    CHECK(normed("C:\\a\\..\\b", "C:\\b"));
    CHECK(normed("/..", "/"));
}
