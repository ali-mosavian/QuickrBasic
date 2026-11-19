/* The keyboard on wasm: the page's keys, through the shared ring (wasm.h), as the host's terminal
 * (host.h) to the host's pkey.c.
 *
 * # How it works
 * The page writes a key at tail and notifies; a read waits on tail with Atomics.wait. A loop that
 * only looks for a key (the editor's, INKEY$'s) is slowed to POLLS a millisecond: it waits that
 * millisecond out on tail, so a key ends the wait.
 */
#include "plat/host/host.h"
#include "wasm.h"

WasmIrq wasm_irq;

[[clang::export_name("irq")]]
WasmIrq *irq(void)
{
    return &wasm_irq;
}

/* Port 60h's bytes (host.h), for the page to push a key's scan code as it goes down and comes up. */
[[clang::export_name("scans")]]
HostScans *scans(void)
{
    return host_scans();
}

void host_term_open(void) {}
void host_term_pause(const bool yes) {}
bool host_term_on(void) { return wasm_irq.keyboard; }
bool host_term_window(void) { return wasm_irq.keyboard; } /* the page's */

static uint32_t tail(void) { return __atomic_load_n(&wasm_irq.tail, __ATOMIC_ACQUIRE); }

uint8_t host_term_shift(void) { return wasm_irq.shift; }

enum { POLLS = 64 };

bool host_term_ready(void)
{
    static double ms;
    static uint32_t polls;

    if (tail() != wasm_irq.head)
        return true;

    const double now = js_now();

    if (now - ms >= 1) {
        ms = now;
        polls = 0;
    } else if (++polls >= POLLS) {
        __builtin_wasm_memory_atomic_wait32(
            (int32_t *)&wasm_irq.tail,
            (int32_t)wasm_irq.head,
            (int64_t)((ms + 1 - now) * 1e6)
        );
    }

    return tail() != wasm_irq.head;
}

uint16_t host_term_key(void)
{
    const uint32_t head = wasm_irq.head;

    while (tail() == head)
        __builtin_wasm_memory_atomic_wait32((int32_t *)&wasm_irq.tail, (int32_t)head, -1);

    const uint16_t k = wasm_irq.keys[head % WASM_NKEY];
    __atomic_store_n(&wasm_irq.head, head + 1, __ATOMIC_RELEASE);
    return k;
}
