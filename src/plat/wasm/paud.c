/* The host's audio stream on wasm: each render played on the device's thread (web/wasm.mjs's
 * stream), the page's AudioWorklet or node's stand-in.
 *
 * # How it works
 * A render is an extension's, its side module loaded here (plat/wasm/pext.c). audio_open hands
 * the device's thread its table index, a block for its frames, a stack, and the lock word: that
 * thread instantiates the side module again on the shared memory and calls the render a quantum at
 * a time, the word held. paud_lock takes the word from this side, nested by a count only this
 * thread touches. A stream stays open for the process.
 */
#include "plat/paud.h"
#include "plat/pmem.h"
#include "wasm.h"

enum { FRAMES = 1024, STACK = 16384, STREAMS = 8 };

uintptr_t pext_alloc(uint32_t n, uint32_t align); /* plat/wasm/pext.c */

static volatile int32_t held; /* the lock word: 1 while either thread holds it */
static uint32_t depth;
static PaudRender *open[STREAMS];
static uint32_t nopen, rate_open;

bool paud_open(PaudRender *const r, uint32_t *const rate, bool *const stereo)
{
    for (uint32_t i = 0; i < nopen; i++)
        if (open[i] == r) {
            *rate = rate_open;
            *stereo = true;
            return true;
        }

    static uintptr_t out, stack; /* the next stream's, kept when one is refused */

    if (nopen == STREAMS)
        return false;
    if (!out)
        out = pext_alloc(FRAMES * 2 * sizeof(int16_t), 16);
    if (!stack)
        stack = pext_alloc(STACK, 16);
    if (!out || !stack)
        return false;

    const uint32_t got =
        js_audio_open((uint32_t)(uintptr_t)r, (void *)out, (void *)(stack + STACK), &held);

    if (!got)
        return false;

    out = stack = 0;
    open[nopen++] = r;
    rate_open = got;
    *rate = got;
    *stereo = true;
    return true;
}

void paud_end(void) {}

uint32_t paud_lock(void)
{
    if (!depth++)
        while (__atomic_exchange_n(&held, 1, __ATOMIC_ACQUIRE))
            ;

    return 0;
}

void paud_unlock(const uint32_t token)
{
    (void)token;

    if (!--depth)
        __atomic_store_n(&held, 0, __ATOMIC_RELEASE);
}
