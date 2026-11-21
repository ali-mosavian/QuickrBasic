/* wasm host: the IDE's stand-in. JS loads the side module into this instance's memory and table;
   the host then calls its entries by function pointer (call_indirect). */
#include "qx.h"

extern void js_say(const char *s, uint32_t n);
extern const QxModule *js_load(const QxHost *h); /* the side module's qx_init_ext, called */

static void say(const char *s, uint32_t n) { js_say(s, n); }
static uint8_t heap[65536] __attribute__((aligned(16)));
static uint32_t used;
static void *alloc(uint32_t n)
{
    void *const p = heap + used;
    used += (n + 15) & ~15u;
    return p;
}
static const QxHost host = {sizeof host, 1, say, alloc};

[[clang::export_name("host_alloc")]] void *host_alloc(uint32_t n, uint32_t align)
{
    used = (used + align - 1) & ~(align - 1);
    return alloc(n);
}

static const QxModule *m;

[[clang::export_name("run")]] int32_t run(void)
{
    m = js_load(&host);
    QxCall c = {.host = &host, .arg = {40, 2}};
    m->entries[0].fn(&c); /* add */
    m->entries[1].fn(&c); /* hello: back into the host through its table */
    c.arg[0] = 1000;
    m->entries[2].fn(&c); /* loud */
    return c.result;
}

/* The render's table index, for the worklet's own instance. */
[[clang::export_name("render_slot")]] uint32_t render_slot(void) { return (uint32_t)(uintptr_t)m->render; }
