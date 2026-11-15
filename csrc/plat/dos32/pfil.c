/* Files and directories on DOS32.
 *
 * # How it works
 * INT 21h's handle calls, DOS/32A passing 32-bit pointers.
 */
#include "dos.h"
#include "plat/pfil.h"

enum { IOCTL_DEVICE = 0x80, NO_DRIVE = 0xFFFF };

static uint32_t ptr(const void *const p) { return (uint32_t)(uintptr_t)p; }

/* INT 21h with r: PFIL_OK, or DOS's error in AX. */
static uint16_t dos(PdosRegs *const r)
{
    return pdos_int(INT_DOS, r) & PDOS_CF ? (uint16_t)r->eax : PFIL_OK;
}

uint16_t pfil_open(const char *const name, const uint8_t access, PfilHandle *const h)
{
    PdosRegs r = {.eax = pdos_fn(DOS_OPEN, access), .edx = ptr(name)};

    const uint16_t err = dos(&r);
    *h = (PfilHandle)r.eax;
    return err;
}

uint16_t pfil_create(const char *const name, PfilHandle *const h)
{
    PdosRegs r = {.eax = pdos_fn(DOS_CREATE, 0), .ecx = 0, .edx = ptr(name)};

    const uint16_t err = dos(&r);
    *h = (PfilHandle)r.eax;
    return err;
}

uint16_t pfil_read(const PfilHandle h, void *const p, const uint32_t n, uint32_t *const got)
{
    PdosRegs r = {
        .eax = pdos_fn(DOS_READ, 0),
        .ebx = h,
        .ecx = n,
        .edx = ptr(p),
    };

    const uint16_t err = dos(&r);
    *got = err ? 0 : r.eax & 0xFFFF;
    return err;
}

uint16_t pfil_write(const PfilHandle h, const void *const p, const uint32_t n, uint32_t *const put)
{
    PdosRegs r = {
        .eax = pdos_fn(DOS_WRITE, 0),
        .ebx = h,
        .ecx = n,
        .edx = ptr(p),
    };

    const uint16_t err = dos(&r);
    *put = err ? 0 : r.eax & 0xFFFF;
    return err;
}

uint16_t pfil_seek(const PfilHandle h, const int32_t off, const uint8_t origin, uint32_t *const pos)
{
    const uint32_t o = (uint32_t)off;
    PdosRegs r = {
        .eax = pdos_fn(DOS_SEEK, origin),
        .ebx = h,
        .ecx = o >> 16,
        .edx = o & 0xFFFF,
    };

    const uint16_t err = dos(&r);

    if (pos && !err)
        *pos = pdos_pair(r.edx, r.eax);

    return err;
}

uint16_t pfil_close(const PfilHandle h)
{
    PdosRegs r = {.eax = pdos_fn(DOS_CLOSE, 0), .ebx = h};

    return dos(&r);
}

enum : PfilHandle { DOS_STDPRN = 4 }; /* the standard printer's handle, PRN */

uint16_t pfil_printer(PfilHandle *const h)
{
    *h = DOS_STDPRN;
    return PFIL_OK;
}

bool pfil_isdev(const PfilHandle h)
{
    PdosRegs r = {.eax = pdos_fn(DOS_IOCTL, 0), .ebx = h};

    return !dos(&r) && r.edx & IOCTL_DEVICE;
}

/* AH=fn on name. */
static uint16_t on_name(const uint8_t fn, const char *const name)
{
    PdosRegs r = {.eax = pdos_fn(fn, 0), .edx = ptr(name)};

    return dos(&r);
}

uint16_t pfil_delete(const char *name) { return on_name(DOS_DELETE, name); }
uint16_t pfil_mkdir(const char *name) { return on_name(DOS_MKDIR, name); }
uint16_t pfil_rmdir(const char *name) { return on_name(DOS_RMDIR, name); }
uint16_t pfil_chdir(const char *name) { return on_name(DOS_CHDIR, name); }

uint16_t pfil_rename(const char *const from, const char *const to)
{
    PdosRegs r = {.eax = pdos_fn(DOS_RENAME, 0), .edx = ptr(from), .edi = ptr(to)};

    return dos(&r);
}

uint16_t pfil_curdir(const uint8_t drive, char *const buf, const uint32_t cap)
{
    char dir[64];
    PdosRegs r = {.eax = pdos_fn(DOS_CURDIR, 0), .edx = drive, .esi = ptr(dir)};

    const uint16_t err = dos(&r);

    if (err)
        return err;

    uint32_t n = 0;

    for (; n + 1 < cap && n < sizeof dir && dir[n]; n++)
        buf[n] = dir[n];

    buf[n] = 0;
    return PFIL_OK;
}

uint8_t pfil_curdrive(void)
{
    PdosRegs r = {.eax = pdos_fn(DOS_CUR_DRIVE, 0)};

    pdos_int(INT_DOS, &r);
    return (uint8_t)r.eax;
}

uint16_t pfil_free(const uint8_t drive, uint32_t *const bytes)
{
    PdosRegs r = {.eax = pdos_fn(DOS_FREE, 0), .edx = drive};

    pdos_int(INT_DOS, &r);

    if ((r.eax & 0xFFFF) == NO_DRIVE)
        return PFIL_BADDRIVE;

    *bytes = (r.eax & 0xFFFF) * (r.ecx & 0xFFFF) * (r.ebx & 0xFFFF);
    return PFIL_OK;
}

uint16_t pfil_getattr(const char *const name, uint8_t *const attr)
{
    PdosRegs r = {.eax = pdos_fn(DOS_ATTR, 0), .edx = ptr(name)};

    const uint16_t err = dos(&r);
    *attr = (uint8_t)r.ecx;
    return err;
}

uint16_t pfil_setattr(const char *const name, const uint8_t attr)
{
    PdosRegs r = {.eax = pdos_fn(DOS_ATTR, 1), .ecx = attr, .edx = ptr(name)};

    return dos(&r);
}

/* f the DTA. */
static void dta(PfilFind *const f)
{
    PdosRegs r = {.eax = pdos_fn(DOS_SET_DTA, 0), .edx = ptr(f)};

    pdos_int(INT_DOS, &r);
}

uint16_t pfil_first(const char *const pattern, const uint8_t attrs, PfilFind *const f)
{
    dta(f);
    PdosRegs r = {.eax = pdos_fn(DOS_FIND_FIRST, 0), .ecx = attrs, .edx = ptr(pattern)};

    return dos(&r);
}

uint16_t pfil_next(PfilFind *const f)
{
    dta(f);
    PdosRegs r = {.eax = pdos_fn(DOS_FIND_NEXT, 0)};

    return dos(&r);
}
