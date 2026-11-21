/* The browser's host, the IDE's stand-in: JS loads the side module into this instance's memory
   and table (load.mjs), then it is driven by function pointer. */
#include "drive.h"

extern void js_say(const char *p, uint32_t len);
extern const QxModule *js_load(const QxHost *h); /* the side module's qx_init_ext, called */

static DriveOut out;
static uint8_t heap[65536] __attribute__((aligned(16)));
static uint32_t used;

[[clang::export_name("host_alloc")]] void *host_alloc(const uint32_t n, const uint32_t align)
{
    used = (used + align - 1) & ~(align - 1);
    void *const p = heap + used;
    used += n;
    return p;
}

static void *alloc(const uint32_t n) { return host_alloc(n, 16); }

static void result_str(QxCall *const c, const char *const p, const uint32_t len)
{
    (void)c;
    const uint32_t n = len < sizeof out.text - 1 ? len : sizeof out.text - 1;
    __builtin_memcpy(out.text, p, n);
    out.text[n] = 0;
}

static bool audio_open(QxAudio *const a)
{
    out.render = a->render;
    return true;
}

static const QxHost host = {sizeof host, QX_ABI, alloc, result_str, audio_open, js_say};

[[clang::export_name("run")]] int32_t run(void) { return drive(js_load(&host), &host, &out); }

/* The render's table index, for the worklet's own instance. */
[[clang::export_name("render_slot")]] uint32_t render_slot(void) { return (uint32_t)(uintptr_t)out.render; }
