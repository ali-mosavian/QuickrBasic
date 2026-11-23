/* Native modules: each .qbx read once a process, its code loaded once, and the host's table.
 *
 * # How it works
 * IMPORT finds foo.qbx (io/locate.c) and the loader reads it whole (natv_open): the header is
 * checked, and its own interface is read as the module's lines from its offset
 * (io/repl.c). Its code is loaded only when the program links (natv_module): this build's slice
 * goes to the platform (plat/pext.h), whose qx_init_foo is called with the host's table. A file
 * stays read and its code loaded for the process, as Python keeps an extension: CHAIN and RUN
 * find them again by path. docs/native-extensions-design.md is the design.
 */
#include "io/native.h"
#include "plat/paud.h"
#include "plat/pext.h"
#include "plat/pfil.h"
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

uint32_t natv_open(Natv *const n, const char *const path)
{
    NatvFile *f = find(n, path);

    if (!f) {
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

        if (!qbxf_read(data, size, pext_tag, &f->qbx))
            f->qbx = (QbxFile){};
    }

    if (!f->qbx.interface || f->qbx.abi != QX_ABI || f->qbx.minor > QX_MINOR)
        fault_qberr(n->fault, QB_NO_ADVANCED);

    return (uint32_t)(f->qbx.interface - f->bytes);
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
    natv_open(n, path);
    NatvFile *const f = find(n, path);

    if (f->tried)
        return f->module;

    f->tried = true;
    char name[64];
    stem(path, name, sizeof name);
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
}

static bool same(const char *const a, const char *const b, const uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        if (upcase((uint8_t)a[i]) != upcase((uint8_t)b[i]) || !a[i])
            return false;

    return !a[len];
}

const QxEntry *natv_entry(const QxModule *const m, const char *const name, const uint32_t len)
{
    for (uint32_t i = 0; i < m->nentries; i++)
        if (same(m->entries[i].name, name, len))
            return &m->entries[i];

    return nullptr;
}

uint32_t natv_type(const QxModule *const m, const char *const name, const uint32_t len)
{
    for (uint32_t i = 0; i < m->ntypes; i++)
        if (same(m->types[i].name, name, len))
            return m->types[i].bytes;

    return 0;
}
