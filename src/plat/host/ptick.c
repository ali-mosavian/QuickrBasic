/* The 18.2 Hz tick on the host: a SIGALRM timer; and the wait for a display's frame. */
#include <signal.h>
#include <stdatomic.h>
#include <sys/time.h>
#include <time.h>

#include "plat/host/host.h"
#include "plat/pclk.h"

enum { NAP_NS = 250000 };

static volatile uint8_t *tick_flag;
static uint8_t tick_bit;

static void on_tick(int sig) { *tick_flag |= tick_bit; }

void pclk_init(volatile uint8_t *const flag, const uint8_t bit)
{
    tick_flag = flag;
    tick_bit = bit;
    signal(SIGALRM, on_tick);
    const struct itimerval every = {{0, 54925}, {0, 54925}};

    setitimer(ITIMER_REAL, &every, nullptr);
}

void pclk_done(void)
{
    const struct itimerval off = {};

    setitimer(ITIMER_REAL, &off, nullptr);
}

/* Polled: the window's thread only counts. */
void host_frame_wait(const uint32_t seen, const uint32_t us)
{
    const uint64_t until = pclk_us() + us;

    while (seen && atomic_load(&host_display()->frames) == seen && pclk_us() < until)
        nanosleep(&(const struct timespec){.tv_nsec = NAP_NS}, nullptr);
}
