#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>

#include "dev.h"
#include "tick.h"

enum { TICK_NANOSECONDS = 54925400, CATCH_UP = 18 }; /* 1 / 18.2065 s; a second's ticks */

/* A weak no-op when the program has no music (play.c is not linked); not an undefined reference, which Mach-O's linker
   rejects. */
__attribute__((weak)) void music_tick(void) {}
__attribute__((weak)) void music_fine_tick(void) {}

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_int started, use_thread, thread_running;
static pthread_t thread;
static uint64_t start_ns, fired; /* the clock at dev_tick_start, the ticks fired since: the lock's */

static void *ticking(void *unused);

static void thread_start(void)
{
    if (!thread_running) {
        thread_running = 1;
        pthread_create(&thread, 0, ticking, 0);
    }
}

static void thread_stop(void)
{
    if (thread_running) {
        thread_running = 0;
        pthread_join(thread, 0);
    }
}

void host_tick_set_thread(int yes)
{
    use_thread = yes;
    if (yes && started)
        thread_start();
    else if (!yes)
        thread_stop();
}

int host_tick_fire(void)
{
    int ran = 0;

    if (pthread_mutex_trylock(&lock) == 0) {
        if (started) {
            music_tick();
            ran = 1;
        }
        pthread_mutex_unlock(&lock);
    }
    return ran;
}

int host_tick_fine_fire(void)
{
    int ran = 0;

    if (pthread_mutex_trylock(&lock) == 0) {
        if (started) {
            music_fine_tick();
            ran = 1;
        }
        pthread_mutex_unlock(&lock);
    }
    return ran;
}

static uint64_t now_ns(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000u + (uint64_t)t.tv_nsec;
}

/* Each tick due by the clock since dev_tick_start, the lock held, as a timer's interrupts come: one held up (a hold, a
   busy machine, a throttled timer) comes late, not never. A stall past CATCH_UP ticks (the machine asleep) is not
   played back at once. */
static void fire_due(void)
{
    const uint64_t due = (now_ns() - start_ns) / TICK_NANOSECONDS;

    if (due - fired > CATCH_UP)
        fired = due - CATCH_UP;
    for (; fired < due; fired++)
        music_tick();
}

void host_tick_due(void)
{
    if (pthread_mutex_trylock(&lock) == 0) {
        if (started)
            fire_due();
        pthread_mutex_unlock(&lock);
    }
}

static void *ticking(void *unused)
{
    (void)unused;
    while (thread_running) {
        pthread_mutex_lock(&lock);
        if (started)
            fire_due();
        const uint64_t next = start_ns + (fired + 1) * TICK_NANOSECONDS;
        pthread_mutex_unlock(&lock);

        const uint64_t now = now_ns(), left = next > now ? next - now : 0;
        const struct timespec wait = {(time_t)(left / 1000000000u), (long)(left % 1000000000u)};

        nanosleep(&wait, 0);
    }
    return 0;
}

void dev_tick_start(void)
{
    pthread_mutex_lock(&lock);
    start_ns = now_ns();
    fired = 0;
    started = 1;
    pthread_mutex_unlock(&lock);
    if (use_thread)
        thread_start();
}

void dev_tick_stop(void)
{
    started = 0;
    thread_stop();
}

void dev_tick_hold(void)
{
    pthread_mutex_lock(&lock);
}

void dev_tick_release(void)
{
    pthread_mutex_unlock(&lock);
}
