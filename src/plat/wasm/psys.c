/* The process on wasm: the page's or node's arguments and exit; STOP is the page's (wasm.h). */
#include <string.h>
#include "plat/psys.h"
#include "wasm.h"

void psys_init(volatile uint8_t *const stop, const uint8_t bit)
{
    wasm_irq.stop_bit = bit;
    __atomic_store_n(&wasm_irq.stop, stop, __ATOMIC_RELEASE);
}

void psys_exit(const uint8_t code) { js_exit(code); }

uint32_t psys_cmdline(char *const buf, const uint32_t cap)
{
    const int n = js_args(buf, cap);
    buf[n < (int)cap ? n : (int)cap - 1] = 0;
    return (uint32_t)strlen(buf);
}

/* DOS's environment as DOSBox starts it, as on the host. */
static const char DOS_ENV[] = "COMSPEC=Z:\\COMMAND.COM\0PATH=Z:\\\0";

uint32_t psys_environ(char *const buf, const uint32_t cap)
{
    const uint32_t n = sizeof DOS_ENV < cap ? sizeof DOS_ENV : cap;
    memcpy(buf, DOS_ENV, n);
    buf[n - 1] = 0;
    return n;
}

uint32_t psys_self(char *const buf, const uint32_t cap)
{
    static const char self[] = "C:\\BASIC.EXE";

    if (cap < sizeof self)
        return 0;

    memcpy(buf, self, sizeof self);
    return sizeof self - 1;
}

bool psys_exec(
    const char *const path,
    const char *const tail,
    const uint8_t len,
    const char *const env,
    const uint32_t envlen
)
{
    return false;
}

void psys_call(const Addr a) {}
