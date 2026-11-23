/* Native modules: each .qbx read once a process, its code loaded once, and the host's table.
 *
 * # How it works
 * IMPORT finds foo.qbx (io/locate.c) and the loader reads it whole (natv_open): the header is
 * checked, and its own interface is read as the module's lines from its offset (io/repl.c). A
 * package's module is a part of its file, named as a file in a folder: gfx.draw is gfx.qbx/draw,
 * as Python's zipimport names one in a zip; its DECLAREs bind to the entries named draw.NAME.
 * The code is loaded only when the program links (natv_module): this build's slice goes to the
 * platform (plat/pext.h), whose qx_init_foo is called with the host's table. A file stays read
 * and its code loaded for the process, as Python keeps an extension: CHAIN and RUN find them
 * again by path. docs/native-extensions-design.md is the design.
 */
#include "io/native.h"
#include "plat/paud.h"
#include "plat/pclk.h"
#include "plat/pext.h"
#include "plat/pfil.h"
#include "plat/pgfx.h"
#include "util/grow.h"

static void *QX_CALL host_alloc(const uint32_t bytes)
{
    void *const p = pmem_resize(nullptr, bytes);

    if (p)
        memset(p, 0, bytes);

    return p;
}

static void QX_CALL host_free(void *const p) { pmem_free(p); }

static bool QX_CALL host_audio(QxAudio *const a)
{
    return paud_open(a->render, &a->rate, &a->stereo);
}

static uint32_t QX_CALL host_lock(void) { return paud_lock(); }

static void QX_CALL host_unlock(const uint32_t token) { paud_unlock(token); }

/* The call's string result: vm/ncall.c copies it once the entry returns. */
static void QX_CALL host_result_str(QxCall *const c, const char *const p, const uint32_t len)
{
    c->result.s = (QxStr){p, len};
}

static bool QX_CALL host_stopped(QxCall *const c)
{
    return *container_of(c, NatvCall, c)->stop & SF_BRK;
}

/* The display's player's, else pages in memory. */
static bool QX_CALL host_gfx_open(QxScreen *const s)
{
    s->out_w = s->w;
    s->out_h = s->h;
    s->hz = 0;
    s->gpu = pgfx_open_played(s->w, s->h);
    return s->gpu || pgfx_open(s->w, s->h, s->depth);
}

static void QX_CALL host_gfx_play(const uint32_t *const list, const uint32_t words)
{
    const PgfxPlayer *const p = pgfx_playing();

    if (p)
        p->play(list, words);
}

static void QX_CALL host_gfx_read(
    const uint32_t texture,
    const int32_t x,
    const int32_t y,
    const int32_t w,
    const int32_t h,
    uint8_t *const out
)
{
    const PgfxPlayer *const p = pgfx_playing();

    if (p)
        p->read(texture, x, y, w, h, out);
}

static void QX_CALL host_gfx_close(void) { pgfx_close(); }

static void *QX_CALL host_gfx_page(uint32_t *const pitch) { return pgfx_page(pitch); }

static void QX_CALL host_gfx_show(const uint32_t *const palette) { pgfx_show(palette); }

static uint64_t QX_CALL host_clock_us(void) { return pclk_us(); }

/* The file at path whole, into the C heap: its bytes, *size; null when it cannot be read. */
static uint8_t *slurp(const char *const path, uint32_t *const size)
{
    PfilHandle h;

    if (pfil_open(path, PFIL_READ, &h) != PFIL_OK)
        return nullptr;

    uint32_t end, got = 0;
    uint8_t *data = nullptr;

    if (pfil_seek(h, 0, PFIL_END, &end) == PFIL_OK && pfil_seek(h, 0, PFIL_SET, &got) == PFIL_OK &&
        (data = pmem_resize(nullptr, end ? end : 1)) && pfil_read(h, data, end, &got) == PFIL_OK &&
        got == end) {
        pfil_close(h);
        *size = end;
        return data;
    }

    pfil_close(h);
    pmem_free(data);
    return nullptr;
}

static void *QX_CALL
host_file_read(const char *const path, const uint32_t len, uint32_t *const bytes)
{
    char name[PFIL_PATH];

    if (len >= sizeof name)
        return nullptr;

    memcpy(name, path, len);
    name[len] = 0;
    return slurp(name, bytes);
}

void natv_init(Natv *const n, Fault *const f)
{
    *n = (Natv){
        .fault = f,
        .host = {
            .size = sizeof(QxHost),
            .abi = QX_ABI,
            .minor = QX_MINOR,
            .alloc = host_alloc,
            .free = host_free,
            .result_str = host_result_str,
            .stopped = host_stopped,
            .audio_open = host_audio,
            .audio_lock = host_lock,
            .audio_unlock = host_unlock,
            .gfx_open = host_gfx_open,
            .gfx_close = host_gfx_close,
            .gfx_page = host_gfx_page,
            .gfx_show = host_gfx_show,
            .clock_us = host_clock_us,
            .file_read = host_file_read,
            .gfx_play = host_gfx_play,
            .gfx_read = host_gfx_read,
        },
    };
}

static NatvFile *find(Natv *const n, const char *const path)
{
    for (uint32_t i = 0; i < n->nfiles; i++)
        if (!strcmp(n->files[i].path, path))
            return &n->files[i];

    return nullptr;
}

/* The end of path's .qbx, a separator after it: null, none. The last, so a folder named x.qbx
   above it is a folder. */
static const char *package_end(const char *const path)
{
    const char *end = nullptr;

    for (const char *p = path; *p; p++)
        if (p > path && p[0] == '.' && upcase((uint8_t)p[1]) == 'Q' &&
            upcase((uint8_t)p[2]) == 'B' && upcase((uint8_t)p[3]) == 'X' &&
            (p[4] == '/' || p[4] == '\\') && p[5])
            end = p + 4;

    return end;
}

bool natv_is(const char *const path) { return qbxf_is(path) || package_end(path); }

const char *natv_part(const char *const path)
{
    const char *const end = package_end(path);
    return end ? end + 1 : path + strlen(path);
}

/* path's file: the .qbx itself, or a part's. */
static void file_of(const char *const path, char out[static PFIL_PATH])
{
    const char *const end = package_end(path);
    const size_t n = end ? (size_t)(end - path) : strlen(path);

    memcpy(out, path, n);
    out[n] = 0;
}

/* The .qbx at path, read once and its header checked (valid false: not a .qbx). Raised: File not
   found; Out of memory. */
static NatvFile *opened(Natv *const n, const char *const path)
{
    NatvFile *f = find(n, path);

    if (f)
        return f;

    uint32_t size;
    uint8_t *const data = slurp(path, &size);
    const uint32_t len = (uint32_t)strlen(path) + 1;
    char *const name = pmem_resize(nullptr, len);

    if (!data || !name) {
        pmem_free(data);
        pmem_free(name);

        if (data)
            fault_error(n->fault, E_OM);

        fault_qberr(n->fault, QB_FILE_NOT_FOUND);
    }

    if (!GROW(n->files, n->filecap, n->nfiles + 1))
        fault_error(n->fault, E_OM);

    memcpy(name, path, len);
    f = &n->files[n->nfiles++];
    *f = (NatvFile){.path = name, .bytes = data, .size = size};

    f->valid = qbxf_read(data, size, pext_tag, &f->qbx);
    return f;
}

uint32_t natv_open(Natv *const n, const char *const path, const char **const file)
{
    char name[PFIL_PATH];
    file_of(path, name);
    const NatvFile *const f = opened(n, name);

    if (!f->valid || f->qbx.abi != QX_ABI || f->qbx.minor > QX_MINOR)
        fault_qberr(n->fault, QB_NO_ADVANCED);

    uint32_t bytes;
    const uint8_t *const text = qbxf_part(f->bytes, f->size, natv_part(path), &bytes);

    if (!text)
        fault_qberr(n->fault, QB_FILE_NOT_FOUND);

    *file = f->path;
    return (uint32_t)(text - f->bytes);
}

bool natv_package(Natv *const n, const char *const path)
{
    if (!qbxf_is(path))
        return false;

    const NatvFile *const f = opened(n, path);
    uint32_t bytes;
    return f->valid && !qbxf_part(f->bytes, f->size, "", &bytes);
}

/* path's file name, no folder nor extension, lower case: its entry's qx_init_<name>. */
static void stem(const char *const path, char *const out, const uint32_t cap)
{
    const char *s = path;

    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\' || *p == ':')
            s = p + 1;

    uint32_t i = 0;

    for (; s[i] && s[i] != '.' && i + 1 < cap; i++)
        out[i] = s[i] >= 'A' && s[i] <= 'Z' ? (char)(s[i] + 32) : s[i];

    out[i] = 0;
}

const QxModule *natv_module(Natv *const n, const char *const path)
{
    const char *file;
    natv_open(n, path, &file);
    NatvFile *const f = find(n, file);

    if (f->tried)
        return f->module;

    f->tried = true;
    char name[64];
    stem(file, name, sizeof name);
    QxInit *const init = f->qbx.slice ? pext_load(f->qbx.slice, f->qbx.nslice, name) : nullptr;
    const QxModule *const m = init ? init(&n->host) : nullptr;

    if (m && m->abi == QX_ABI && m->size >= sizeof(QxModule))
        f->module = m;

    return f->module;
}

void natv_start(Natv *const n)
{
    natv_stop(n);
    n->running = true;

    for (uint32_t i = 0; i < n->nfiles; i++)
        if (n->files[i].module && n->files[i].module->start)
            n->files[i].module->start();
}

void natv_stop(Natv *const n)
{
    if (!n->running)
        return;

    n->running = false;

    for (uint32_t i = 0; i < n->nfiles; i++)
        if (n->files[i].module && n->files[i].module->stop)
            n->files[i].module->stop();

    pgfx_close(); /* a screen an extension left open ends with the run */
}

/* a, an entry's or a type's name, is part.name (part "": name), compared without case. */
static bool same(
    const char *const a,
    const char *const part,
    const char *const name,
    const uint32_t len
)
{
    const uint32_t plen = (uint32_t)strlen(part), at = plen ? plen + 1 : 0;

    for (uint32_t i = 0; i < plen; i++)
        if (upcase((uint8_t)a[i]) != upcase((uint8_t)part[i]) || !a[i])
            return false;

    if (plen && a[plen] != '.')
        return false;

    for (uint32_t i = 0; i < len; i++)
        if (upcase((uint8_t)a[at + i]) != upcase((uint8_t)name[i]) || !a[at + i])
            return false;

    return !a[at + len];
}

const QxEntry *natv_entry(
    const QxModule *const m,
    const char *const part,
    const char *const name,
    const uint32_t len
)
{
    for (uint32_t i = 0; i < m->nentries; i++)
        if (same(m->entries[i].name, part, name, len))
            return &m->entries[i];

    return nullptr;
}

uint32_t natv_type(
    const QxModule *const m,
    const char *const part,
    const char *const name,
    const uint32_t len
)
{
    for (uint32_t i = 0; i < m->ntypes; i++)
        if (same(m->types[i].name, part, name, len))
            return m->types[i].bytes;

    return 0;
}
