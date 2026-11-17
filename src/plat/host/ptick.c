/* The 18.2 Hz tick on the host: a SIGALRM timer. */
#include <signal.h>
#include <sys/time.h>
#include "plat/pclk.h"

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
