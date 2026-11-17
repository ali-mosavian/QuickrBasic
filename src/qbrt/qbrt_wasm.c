/* The runtime's device on wasm: the headless one, as on the host (qbrt_host.c), its framebuffer
   the page's to show. One thread: the music's tick is a wait's, in real time, or each wait a tick. */
#include <time.h>
#include "plat/pfil.h"
#include "plat/psys.h"
#include "qbrt/qbrt.h"
#include "headless.h"
#include "wasm.h"

static constexpr double TICK_MS = 1000 / 18.2065;
enum { HUNDREDTHS_A_TICK = 6 };

static bool realtime = true;
static double next_tick; /* js_now()'s, ms */

int host_tick_fire(void);

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

/* To the next tick's time, slept (Atomics.wait on a word nothing notifies), then the tick. */
void qbrt_tick_wait(void)
{
    if (!realtime) {
        headless_run(HUNDREDTHS_A_TICK);
        return;
    }

    static int32_t never;
    const double now = js_now();

    if (now < next_tick)
        __builtin_wasm_memory_atomic_wait32(&never, 0, (int64_t)((next_tick - now) * 1e6));

    next_tick = (now > next_tick ? now : next_tick) + TICK_MS;
    host_tick_fire();
}

/* The OS calls the runtime makes (llrm_os.h): its console's output, its exit. */
int32_t llrm_os_write_file(const int handle, const unsigned char *const data, const size_t count)
{
    uint32_t put;
    return pfil_write((PfilHandle)handle, data, (uint32_t)count, &put) ? -1 : (int32_t)put;
}

void llrm_os_exit(const unsigned char code) { psys_exit(code); }

/* The program's graphics screen kept while the editor shows: the runtime's framebuffer is not the
   text pages', so nothing is copied; the display shows the text instead (frame.c). */
uint32_t qbrt_shot_size(void) { return 1; }
void qbrt_shot_take(void *const shot) { wasm_text_shown = true; }
void qbrt_shot_show(const void *const shot) { wasm_text_shown = false; }

/* The speaker: each tone to the page as well (--wrap). */
void __real_dev_tone(unsigned hertz);

void __wrap_dev_tone(const unsigned hertz)
{
    js_tone(hertz);
    __real_dev_tone(hertz);
}
