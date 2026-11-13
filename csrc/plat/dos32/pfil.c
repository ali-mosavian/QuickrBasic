/* Files and directories on DOS32: INT 21h's handle calls, DOS/32A passing 32-bit pointers. */
#include "dos.h"
#include "plat/pfil.h"

static uint32_t ptr(const void *p) { return (uint32_t)(uintptr_t)p; }

/* INT 21h with r: PFIL_OK, or DOS's error in AX. */
static uint16_t dos(PdosRegs *r)
{
    return pdos_int(0x21, r) & PDOS_CF ? (uint16_t)r->eax : PFIL_OK;
}

uint16_t pfil_open(const char *name, uint8_t access, PfilHandle *h)
{
    PdosRegs r = {.eax = 0x3D00u | access, .edx = ptr(name)};
    uint16_t err = dos(&r);
    *h = (PfilHandle)r.eax;
    return err;
}

uint16_t pfil_create(const char *name, PfilHandle *h)
{
    PdosRegs r = {.eax = 0x3C00, .ecx = 0, .edx = ptr(name)};
    uint16_t err = dos(&r);
    *h = (PfilHandle)r.eax;
    return err;
}

uint16_t pfil_read(PfilHandle h, void *p, uint32_t n, uint32_t *got)
{
    PdosRegs r = {.eax = 0x3F00, .ebx = h, .ecx = n, .edx = ptr(p)};
    uint16_t err = dos(&r);
    *got = err ? 0 : r.eax & 0xFFFF;
    return err;
}

uint16_t pfil_write(PfilHandle h, const void *p, uint32_t n, uint32_t *put)
{
    PdosRegs r = {.eax = 0x4000, .ebx = h, .ecx = n, .edx = ptr(p)};
    uint16_t err = dos(&r);
    *put = err ? 0 : r.eax & 0xFFFF;
    return err;
}

uint16_t pfil_seek(PfilHandle h, int32_t off, uint8_t origin, uint32_t *pos)
{
    uint32_t o = (uint32_t)off;
    PdosRegs r = {.eax = 0x4200u | origin, .ebx = h, .ecx = o >> 16, .edx = o & 0xFFFF};
    uint16_t err = dos(&r);
    if (pos && !err)
        *pos = (r.edx & 0xFFFF) << 16 | (r.eax & 0xFFFF);
    return err;
}

uint16_t pfil_close(PfilHandle h)
{
    PdosRegs r = {.eax = 0x3E00, .ebx = h};
    return dos(&r);
}

bool pfil_isdev(PfilHandle h)
{
    PdosRegs r = {.eax = 0x4400, .ebx = h};
    return !dos(&r) && r.edx & 0x80;
}

/* AH=fn on name. */
static uint16_t on_name(uint16_t fn, const char *name)
{
    PdosRegs r = {.eax = (uint32_t)fn << 8, .edx = ptr(name)};
    return dos(&r);
}

uint16_t pfil_delete(const char *name) { return on_name(0x41, name); }
uint16_t pfil_mkdir(const char *name) { return on_name(0x39, name); }
uint16_t pfil_rmdir(const char *name) { return on_name(0x3A, name); }
uint16_t pfil_chdir(const char *name) { return on_name(0x3B, name); }

uint16_t pfil_rename(const char *from, const char *to)
{
    PdosRegs r = {.eax = 0x5600, .edx = ptr(from), .edi = ptr(to)};
    return dos(&r);
}

uint16_t pfil_curdir(uint8_t drive, char *buf, uint32_t cap)
{
    char dir[64];
    PdosRegs r = {.eax = 0x4700, .edx = drive, .esi = ptr(dir)};
    uint16_t err = dos(&r);
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
    PdosRegs r = {.eax = 0x1900};
    pdos_int(0x21, &r);
    return (uint8_t)r.eax;
}

uint16_t pfil_free(uint8_t drive, uint32_t *bytes)
{
    PdosRegs r = {.eax = 0x3600, .edx = drive};
    pdos_int(0x21, &r);
    if ((r.eax & 0xFFFF) == 0xFFFF)
        return PFIL_BADDRIVE;
    *bytes = (r.eax & 0xFFFF) * (r.ecx & 0xFFFF) * (r.ebx & 0xFFFF);
    return PFIL_OK;
}

uint16_t pfil_getattr(const char *name, uint8_t *attr)
{
    PdosRegs r = {.eax = 0x4300, .edx = ptr(name)};
    uint16_t err = dos(&r);
    *attr = (uint8_t)r.ecx;
    return err;
}

uint16_t pfil_setattr(const char *name, uint8_t attr)
{
    PdosRegs r = {.eax = 0x4301, .ecx = attr, .edx = ptr(name)};
    return dos(&r);
}

/* f the DTA. */
static void dta(PfilFind *f)
{
    PdosRegs r = {.eax = 0x1A00, .edx = ptr(f)};
    pdos_int(0x21, &r);
}

uint16_t pfil_first(const char *pattern, uint8_t attrs, PfilFind *f)
{
    dta(f);
    PdosRegs r = {.eax = 0x4E00, .ecx = attrs, .edx = ptr(pattern)};
    return dos(&r);
}

uint16_t pfil_next(PfilFind *f)
{
    dta(f);
    PdosRegs r = {.eax = 0x4F00};
    return dos(&r);
}
