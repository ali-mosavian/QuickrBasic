/* The clock on the host: local time; the 18.2 Hz tick a SIGALRM timer. */
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include "plat/pclk.h"

enum : uint32_t { JDAY = 5184000, DAY_SECONDS = 86400 };
static constexpr double TICKS_PER_SECOND = 1193180.0 / 65536.0;

static volatile uint8_t *tick_flag;
static uint8_t tick_bit;
static int32_t day_offset; /* seconds the clock was set ahead */
static uint32_t toff;      /* the jiffy clock's offset */

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

static struct tm local_now(double *const seconds)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    const time_t t = ts.tv_sec + day_offset;
    struct tm tm;
    localtime_r(&t, &tm);
    *seconds = tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec + ts.tv_nsec / 1e9;
    return tm;
}

uint32_t pclk_ticks(void)
{
    double s;
    local_now(&s);
    return (uint32_t)(s * TICKS_PER_SECOND);
}

void pclk_time(PclkTime *const t)
{
    double s;
    const struct tm tm = local_now(&s);
    *t = (PclkTime){
        .hour = (uint8_t)tm.tm_hour,
        .min = (uint8_t)tm.tm_min,
        .sec = (uint8_t)tm.tm_sec,
        .hund = (uint8_t)((s - (uint32_t)s) * 100),
    };
}

void pclk_date(PclkDate *const d)
{
    double s;
    const struct tm tm = local_now(&s);
    *d = (PclkDate){.year = (uint16_t)(tm.tm_year + 1900),
                    .month = (uint8_t)(tm.tm_mon + 1),
                    .day = (uint8_t)tm.tm_mday};
}

/* The host's clock is not set: the change is kept as an offset. */
bool pclk_set_time(const PclkTime *const t)
{
    if (t->hour > 23 || t->min > 59 || t->sec > 59 || t->hund > 99)
        return false;

    double s;
    local_now(&s);
    day_offset += (int32_t)(t->hour * 3600 + t->min * 60 + t->sec) - (int32_t)s;
    return true;
}

bool pclk_set_date(const PclkDate *const d)
{
    if (d->year < 1980 || d->year > 2099 || !d->month || d->month > 12 || !d->day || d->day > 31)
        return false;

    double s;
    struct tm tm = local_now(&s);
    const time_t now = mktime(&tm);
    tm.tm_year = d->year - 1900;
    tm.tm_mon = d->month - 1;
    tm.tm_mday = d->day;
    day_offset += (int32_t)(mktime(&tm) - now);
    return true;
}

/* The ticks as jiffies: ticks * 3295 / 1000, as DOS32's. */
static uint32_t jiffies(void)
{
    const uint32_t t = pclk_ticks();
    return t * 3 + t * 295 / 1000;
}

uint32_t pclk_jiffies(void) { return (jiffies() + toff) % JDAY; }
void pclk_set_jiffies(const uint32_t j) { toff = j - jiffies() + JDAY; }
