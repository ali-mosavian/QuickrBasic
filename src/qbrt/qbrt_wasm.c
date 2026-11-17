/* The runtime's device on wasm: the headless one, as on the host (qbrt_host.c), its framebuffer
   the page's to show. The music's tick is the page's thread's, as the host's tick thread. */
#include <time.h>
#include "plat/pfil.h"
#include "plat/psys.h"
#include "qbrt/qbrt.h"
#include "headless.h"
#include "wasm.h"

enum { HUNDREDTHS_A_TICK = 6, WAIT_NANOSECONDS = 1000000 };

static bool realtime = true;

void qbrt_clock_set(const PclkTime *const t)
{
    headless_clock_set(((t->hour * 60L + t->min) * 60 + t->sec) * 100 + t->hund, 0);
    headless_set_real_time(1);
}

void qbrt_device(void)
{
    headless_reset();
    PclkTime now;
    pclk_time(&now);
    qbrt_clock_set(&now);
    headless_set_stdout_is_screen(wasm_irq.keyboard); /* the page's: the screen; node's: a stream */
    headless_set_tick_thread(0);
}

void qbrt_realtime(const bool yes) { realtime = yes; }

/* A wait for the music: a millisecond, the tick the page's thread's (host_tick_fire, wasm.mjs); not in
   real time, each wait a tick. */
void qbrt_tick_wait(void)
{
    static int32_t never;

    if (realtime)
        __builtin_wasm_memory_atomic_wait32(&never, 0, WAIT_NANOSECONDS);
    else
        headless_run(HUNDREDTHS_A_TICK);
}

/* The OS calls the runtime makes (llrm_os.h): its console's output, its exit. */
int32_t llrm_os_write_file(const int handle, const unsigned char *const data, const size_t count)
{
    uint32_t put;
    return pfil_write((PfilHandle)handle, data, (uint32_t)count, &put) ? -1 : (int32_t)put;
}

void llrm_os_exit(const unsigned char code) { psys_exit(code); }

/* The speaker and the voices: each to the page as well (--wrap). */
void __real_dev_tone(unsigned hertz);
void __real_dev_voice(unsigned voice, const DevVoice *v);

void __wrap_dev_voice(const unsigned voice, const DevVoice *const v)
{
    js_voice(voice, v);
    __real_dev_voice(voice, v);
}

void __wrap_dev_tone(const unsigned hertz)
{
    js_tone(hertz);
    __real_dev_tone(hertz);
}
