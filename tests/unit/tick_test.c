/* The host's clock tick (llrm's dev/host/tick.c) in real time. */
#include <time.h>

#include "qbrt/qbrt.h"
#include "unit.h"

/* llrm's dev/host/tick.h, dev/dev.h and music.h. */
void host_tick_set_thread(int yes);
void dev_tick_start(void);
void dev_tick_hold(void);
void dev_tick_release(void);
unsigned long music_end(unsigned voice);

enum { VOICE = 31, FINE = 16 }; /* a voice no test plays: its end the clock; MUSIC_FINE */

/* ms milliseconds slept: a signal (the runtime's) cuts a sleep short, so the rest is slept again. */
static void wait_ms(const long ms)
{
    struct timespec left = {ms / 1000, ms % 1000 * 1000000};

    while (nanosleep(&left, &left))
        ;
}

/* The tick started (left running, as music.c thinks it is), the ticks a synthesizer's recent parts
   stand for (music.c's FINE_KEPT) gone by: the clock from here. */
static unsigned long started(void)
{
    dev_tick_start();
    wait_ms(150);
    qbrt_tick_poll();
    return music_end(VOICE);
}

/* The ticks since from. */
static unsigned long ticks_since(const unsigned long from)
{
    return (music_end(VOICE) - from) / FINE;
}

/* A tick held up (the program holding the tick's lock, a busy machine) was lost: the thread slept
   a tick's length after each, so music ran slow. 0.55 s is 10 ticks however late they come. */
TEST(host_tick_keeps_real_time)
{
    qbrt_realtime(true);
    const unsigned long from = started();
    dev_tick_hold();
    wait_ms(300);
    dev_tick_release();
    wait_ms(250);
    const unsigned long ticks = ticks_since(from);
    CHECK(ticks >= 9 && ticks <= 11);
}

/* A program waiting on the music (beat(), PLAY(n)) had only the thread's ticks: where the system
   held the thread's sleeps back (a background process's timers), a song stood still and
   tests/qbrun/song.bas ran past its 3 s on CI. Its own look at the music now ticks what is due. */
TEST(host_tick_polled_keeps_real_time)
{
    qbrt_realtime(true);
    host_tick_set_thread(0); /* the thread held back for good */
    const unsigned long from = started();
    wait_ms(550);
    qbrt_tick_poll();
    const unsigned long ticks = ticks_since(from);
    qbrt_realtime(true);
    CHECK(ticks >= 9 && ticks <= 11);
}
