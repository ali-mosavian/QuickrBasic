/* Where a module's file is.
 *
 * # How it works
 * A module's name `lib.gfx` is the file `lib/gfx.bas`, else the native module `lib/gfx.qbx`, or for
 * FROM the package folder `lib/gfx/`, under the first root that has it: the main program's folder,
 * each of BASICLIB's folders (';'-separated), then `lib/` beside the interpreter. FROM's leading
 * dots name one root instead: the importing file's folder, a folder up for each dot past the first.
 * A Quickr module is a `.bas` alone, its first line saying so (qr/qr.h).
 */
#include "io/locate.h"
#include "io/path.h"
#include "plat/psys.h"
#include <string.h>

/* The root's n bytes, then name n's parts as folders, then tail: to out, its "." and ".." parts
   taken out. false: too long. */
static bool place(
    char *const out,
    const char *const root,
    const uint32_t n,
    const ImptName *const m,
    const char *const tail
)
{
    const uint32_t len = m->len, t = (uint32_t)strlen(tail);

    if (n + len + t + 1 >= PFIL_PATH)
        return false;

    memcpy(out, root, n);

    for (uint32_t i = 0; i < len; i++)
        out[n + i] = m->text[i] == '.' ? '/' : (char)m->text[i];

    memcpy(out + n + len, tail, t + 1);
    path_norm(out);
    return true;
}

/* out names a folder: then it ends with a separator. DOS finds a folder's attributes only
   without one. */
static bool folder(char *const out)
{
    uint32_t n = (uint32_t)strlen(out);

    while (n && (out[n - 1] == '/' || out[n - 1] == '\\'))
        out[--n] = 0;

    if (!path_isdir(n ? out : ".") || n + 1 >= PFIL_PATH)
        return false;
    if (n) {
        out[n] = '/';
        out[n + 1] = 0;
    }

    return true;
}

/* Name n under the root's n bytes: its file, of extension ext, else a .bas then a .qbx; else (dir
   not null) its folder. */
static bool under(
    const char *const root,
    const uint32_t n,
    const ImptName *const m,
    const char *const ext,
    char *const out,
    bool *const dir
)
{
    if (ext && m->len && place(out, root, n, m, ext) && path_exists(out))
        return true;
    if (!ext && m->len && place(out, root, n, m, ".bas") && path_exists(out))
        return true;
    if (!ext && m->len && place(out, root, n, m, ".qbx") && path_exists(out))
        return true;

    return dir && place(out, root, n, m, "") && (*dir = folder(out));
}

/* FROM's dots' root: the importing file's folder, then ".." for each dot past the first. */
static bool dotted(const Locate *const l, const ImptName *const m, char *const out, bool *const dir)
{
    char root[PFIL_PATH];
    uint32_t n = path_dir(l->from);

    if (n + 3u * m->up >= PFIL_PATH)
        return false;

    memcpy(root, l->from, n);

    for (uint32_t i = 1; i < m->up; i++, n += 3)
        memcpy(root + n, "../", 3);

    return under(root, n, m, l->ext, out, dir);
}

bool locate(
    const Locate *const l,
    const ImptName *const m,
    char out[static PFIL_PATH],
    bool *const dir
)
{
    if (dir)
        *dir = false;
    if (m->up)
        return dotted(l, m, out, dir);
    if (under(l->main, path_dir(l->main), m, l->ext, out, dir))
        return true;

    for (const char *p = l->lib; p && *p;) { /* BASICLIB's folders */
        const char *const end = strchr(p, ';');
        const uint32_t n = end ? (uint32_t)(end - p) : (uint32_t)strlen(p);
        char root[PFIL_PATH];

        if (n && n + 1 < PFIL_PATH) {
            memcpy(root, p, n);
            root[n] = '/';

            if (under(root, n + (root[n - 1] != '/' && root[n - 1] != '\\'), m, l->ext, out, dir))
                return true;
        }

        p = end ? end + 1 : p + n;
    }

    char self[PFIL_PATH];

    if (!psys_self(self, sizeof self)) /* the interpreter's folder unknown */
        return false;

    const uint32_t n = path_dir(self);

    if (n + 4 >= PFIL_PATH)
        return false;

    memcpy(self + n, "lib/", 5);
    return under(self, n + 4, m, l->ext, out, dir);
}

bool locate_in(
    const char *const dir,
    const ImptName *const m,
    const char *const ext,
    char out[static PFIL_PATH]
)
{
    return under(dir, (uint32_t)strlen(dir), m, ext, out, nullptr);
}
