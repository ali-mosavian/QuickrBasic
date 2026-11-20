/* The program's source files and its modules: io/repl.c loads them; the compiler asks whose a
 * line is.
 *
 * # How it works
 * A file's number is its lines' Line.file: the main program's 0, then each as it is read, an
 * include's or a module's. A file knows its path and its module: the main program's (0) for its
 * own lines and its includes', else the module whose file it is or includes it. A module is
 * loaded once however many import it: paths compare without case. The imports are edges; a
 * depth-first walk from the main program orders the modules for their declarations.
 */
#include "mem/module.h"
#include "util/grow.h"

void modu_init(Modu *const mu, Fault *const f) { mu->fault = f; }

/* path's copy in the text: its offset. */
static uint32_t keep(Modu *const mu, const char *const path)
{
    const uint32_t n = (uint32_t)strlen(path) + 1, at = mu->ntext;

    if (!GROW(mu->text, mu->textcap, at + n))
        fault_error(mu->fault, E_OM);

    memcpy(mu->text + at, path, n);
    mu->ntext += n;
    return at;
}

void modu_clear(Modu *const mu, const char *const main)
{
    mu->nfiles = mu->nmods = mu->ntext = mu->nedges = 0;
    modu_module(mu, main);
}

/* c, its case and separator as any other's. */
static uint8_t fold(const char c) { return c == '\\' ? '/' : upcase((uint8_t)c); }

static bool same(const char *a, const char *b)
{
    for (; *a && fold(*a) == fold(*b); a++, b++)
        ;

    return fold(*a) == fold(*b);
}

uint32_t modu_module(Modu *const mu, const char *const path)
{
    for (uint32_t m = 0; m < mu->nmods; m++)
        if (same(mu->text + mu->mods[m].path, path))
            return m;

    if (!GROW(mu->mods, mu->modcap, mu->nmods + 1))
        fault_error(mu->fault, E_OM);

    mu->mods[mu->nmods] = (ModuMod){keep(mu, path), 0};
    return mu->nmods++;
}

void modu_import(Modu *const mu, const uint32_t from, const uint32_t to)
{
    if (!GROW(mu->edges, mu->edgecap, mu->nedges + 1))
        fault_error(mu->fault, E_OM);

    mu->edges[mu->nedges++] = (ModuEdge){from, to};
}

void modu_top(Modu *const mu, const uint32_t m, const uint32_t line) { mu->mods[m].top = line; }

uint32_t modu_top_of(const Modu *const mu, const uint32_t m) { return mu->mods[m].top; }

/* Module m after those it imports not yet seen, from n on: past them. */
static uint32_t after(Modu *const mu, const uint32_t m, uint32_t n)
{
    mu->seen[m] = 1;

    for (uint32_t i = 0; i < mu->nedges; i++)
        if (mu->edges[i].from == m && !mu->seen[mu->edges[i].to])
            n = after(mu, mu->edges[i].to, n);

    mu->order[n] = m;
    return n + 1;
}

const uint32_t *modu_order(Modu *const mu)
{
    if (!GROW(mu->order, mu->ordercap, mu->nmods) || !GROW(mu->seen, mu->seencap, mu->nmods))
        fault_error(mu->fault, E_OM);

    memset(mu->seen, 0, mu->nmods);
    uint32_t n = after(mu, 0, 0);

    for (uint32_t m = 1; m < mu->nmods; m++) /* none unreached: each was found by an import */
        if (!mu->seen[m])
            n = after(mu, m, n);

    return mu->order;
}

uint32_t modu_count(const Modu *const mu) { return mu->nmods; }

const char *modu_module_path(const Modu *const mu, const uint32_t m)
{
    return m < mu->nmods ? mu->text + mu->mods[m].path : "";
}

void modu_file(Modu *const mu, const uint32_t file, const char *const path, const uint32_t m)
{
    if (!GROW(mu->files, mu->filecap, file + 1))
        fault_error(mu->fault, E_OM);

    mu->files[file] = (ModuFile){keep(mu, path), m};

    if (file >= mu->nfiles)
        mu->nfiles = file + 1;
}

const char *modu_path(const Modu *const mu, const uint32_t file)
{
    return file < mu->nfiles ? mu->text + mu->files[file].path : "";
}

uint32_t modu_of(const Modu *const mu, const uint32_t file)
{
    return file < mu->nfiles ? mu->files[file].module : 0;
}
