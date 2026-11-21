/* The runtime's device on the host: llrm's headless one, a framebuffer tests read (video.h), a
   speaker that is a log. Standard output stays a stream: the screen is the framebuffer only where a
   test says so, or a display shows it (QBRT_SCREEN, plat/view). The music's tick is a thread in real time; else each wait for it is a tick. */
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include "qbrt/qbrt.h"
#include "headless.h"
#include "tick.h"

#ifndef QBRT_SCREEN
#define QBRT_SCREEN 0
#endif

enum { HUNDREDTHS_A_TICK = 6, WAIT_NANOSECONDS = 1000000 };

static bool realtime = true;
static bool polled; /* PLAY(n) looked at since the last wait */

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
    headless_set_stdout_is_screen(QBRT_SCREEN);
    headless_set_tick_thread(realtime);
    polled = false;
}

void qbrt_realtime(const bool yes)
{
    realtime = yes;
    polled = false;
    headless_set_tick_thread(yes);
}

void qbrt_tick_poll(void)
{
    if (realtime)
        host_tick_due();
    else if (polled)
        headless_run(1);

    polled = true;
}

void qbrt_tick_wait(void)
{
    polled = false;

    if (realtime) {
        const struct timespec wait = {0, WAIT_NANOSECONDS};

        nanosleep(&wait, 0);
    } else
        headless_run(HUNDREDTHS_A_TICK);
}
