/* Native code on DOS: an extension's DLL, or a driver's (accel/load.c), placed in the C side's
 * heap and run where it lies.
 *
 * # How it works
 * The slice is an i386 DLL (util/image.c). A block is taken from the heap, aligned to 16, the DLL
 * placed in it as if it lay at the block's DS offset, and its entry, an export, called there. That
 * needs CS to see what DS does: the same base, and a limit past the block, as DOS/32A's flat
 * segments have; else it is not run. The block is the process's.
 */
#include "dos.h"
#include "plat/pext.h"
#include "plat/pmem.h"
#include "util/image.h"
#include <string.h>

enum { ALIGN = 16, ENTRY_MOST = 32 };

const char pext_tag[] = "dos-i386";

/* Code at DS offset at, n bytes, runs: CS's base is DS's and its limit past them. */
static bool runnable(const uint32_t at, const uint32_t n)
{
    PdosRegs r = {.eax = DPMI_SEG_BASE, .ebx = pdos_cs()};

    if (pdos_int(INT_DPMI, &r) & PDOS_CF)
        return false;

    return pdos_pair(r.ecx, r.edx) == pdos_ds_base() && pdos_limit(pdos_cs()) >= at + n - 1;
}

#ifdef PEXT_LINKED
/* bench.py profile's build: gfx's slice linked into the program, so the program's map names it. */
const QxModule *QX_CALL qx_init_gfx(const QxHost *host);
#endif

void *pdos_place(const void *const dll, const uint32_t n, const char *const entry)
{
    Image im;
    uint32_t start;

    if (!imag_read(dll, n, &im) || !imag_export(&im, entry, &start))
        return nullptr;

    uint8_t *const raw = pmem_resize(nullptr, im.size + ALIGN - 1);

    if (!raw)
        return nullptr;

    const uint32_t at = ((uint32_t)(uintptr_t)raw + ALIGN - 1) & ~(uint32_t)(ALIGN - 1);

    if (!runnable(at, im.size)) {
        pmem_free(raw);
        return nullptr;
    }

    imag_place(&im, (uint8_t *)(uintptr_t)at, at);
    return (void *)(uintptr_t)(at + start);
}

QxInit *pext_load(const void *const slice, const uint32_t bytes, const char *const name)
{
#ifdef PEXT_LINKED
    if (!strcmp(name, "gfx"))
        return qx_init_gfx;
#endif

    static const char INIT[] = "qx_init_";
    char entry[ENTRY_MOST];
    const size_t n = strlen(name);

    if (n >= sizeof entry - sizeof INIT + 1)
        return nullptr;

    memcpy(entry, INIT, sizeof INIT - 1);
    memcpy(entry + sizeof INIT - 1, name, n + 1);
    return (QxInit *)pdos_place(slice, bytes, entry);
}
