/* Native code on wasm: an extension's side module instantiated on this module's memory and table.
 *
 * # How it works
 * The slice is a side module (wasm-ld -shared). The JS host's ext_load (web/wasm.mjs) compiles it
 * and reads from its dylink.0 the memory and table it needs; it takes the memory from here
 * (pext_alloc), grows the table, and instantiates it on this module's memory, table and stack
 * pointer. Its data relocated and constructors run, qx_init_<name> is put in the table: that index
 * is the function pointer. It runs on this thread alone: another instance (the display's, the
 * music's) has a table of its own.
 */
#include "plat/pext.h"
#include "plat/pmem.h"
#include "wasm.h"

const char pext_tag[] = "wasm32";

/* n bytes, zeroed, at a multiple of align (a power of 2), for a side module's data: the process's.
   0, none left. */
[[clang::export_name("pext_alloc")]]
uintptr_t pext_alloc(const uint32_t n, const uint32_t align)
{
    uint8_t *const raw = pmem_resize(nullptr, n + align);

    if (!raw)
        return 0;

    const uintptr_t at = ((uintptr_t)raw + align - 1) & ~(uintptr_t)(align - 1);
    memset((void *)at, 0, n);
    return at;
}

QxInit *pext_load(const void *const slice, const uint32_t bytes, const char *const name)
{
    const uint32_t f = js_ext_load(slice, bytes, name, strlen(name));
    return f ? (QxInit *)(uintptr_t)f : nullptr;
}
