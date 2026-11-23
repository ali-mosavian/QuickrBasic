/* The speaker's statements on llrm's music queue (comp/sndstmt.c, vm/sound.c, ON PLAY in
   vm/event.c), the headless speaker's log read. */
#include "session.h"
#include "qbrt/qbrt.h"
#include "unit.h"

/* llrm's headless device (dev/headless/headless.h). */
typedef struct {
    unsigned hertz;
    long clock, tick;
} HeadlessTone;
size_t headless_tone_count(void);
HeadlessTone headless_tone(size_t index);
void headless_run(long hundredths);

/* The tones logged from first on, their hertz; how many. */
static size_t tones(const size_t first, unsigned *const hertz, const size_t max)
{
    size_t n = 0;

    for (size_t i = first; i < headless_tone_count() && n < max; i++)
        hertz[n++] = headless_tone(i).hertz;

    return n;
}

/* A PLAY on a quiet voice starts its time afresh. The part of a tick its last note left over carried into the next
   program: after PLAY "T200 L32 C" the same eighth at T180 lasted 3 ticks, not its 2 (2655 thousandths). */
TEST(play_starts_its_time_afresh)
{
    static const char *const before[] = {"10 PLAY \"MF T200 L32 C\"", nullptr};
    static const char *const lines[] = {"10 PLAY \"MF T180 L8 O3 C D\"", nullptr};

    qbrt_realtime(false);
    CHECK(unit_run(S, before) == ST_OK);
    headless_run(100);
    const size_t first = headless_tone_count();
    CHECK(unit_run(S, lines) == ST_OK);
    headless_run(100);
    qbrt_realtime(true);
    CHECK(headless_tone_count() - first >= 2 && headless_tone(first).hertz == 262 && !headless_tone(first + 1).hertz);
    CHECK(headless_tone(first + 1).tick - headless_tone(first).tick == 2);
}

/* Foreground notes follow each other as background ones do. Each waited for the one before to stop sounding, so one
   with no quiet after it (ML) started a tick late: 55 ms of silence where legato joins. */
TEST(play_foreground_notes_join)
{
    static const char *const lines[] = {"10 PLAY \"MF T180 ML L8 O3 A B\"", nullptr};

    qbrt_realtime(false);
    const size_t first = headless_tone_count();
    CHECK(unit_run(S, lines) == ST_OK);
    headless_run(100);
    qbrt_realtime(true);
    CHECK(headless_tone_count() - first == 4 && headless_tone(first).hertz == 440);
    CHECK(headless_tone(first + 2).hertz == 494);
    CHECK(headless_tone(first + 2).tick == headless_tone(first + 1).tick);
}

/* A foreground SOUND waits for the one sounding; each sounds, then the speaker is quiet. */
TEST(sound_plays_in_turn)
{
    static const char *const lines[] = {"10 SOUND 440, 2: SOUND 880, 3", nullptr};

    qbrt_realtime(false);
    const size_t first = headless_tone_count();

    CHECK(unit_run(S, lines) == ST_OK);
    headless_run(100);
    unsigned hz[8];
    const size_t n = tones(first, hz, 8);
    CHECK(n == 4 && hz[0] == 440 && hz[1] == 0 && hz[2] == 880 && hz[3] == 0);
    qbrt_realtime(true);
}

/* Background music queues its notes and goes on: PLAY(n) counts them. */
TEST(play_background_waits_for_none)
{
    static const char *const lines[] = {"10 PLAY \"MB L4 CDE\": PRINT PLAY(0)", nullptr};

    qbrt_realtime(false);

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 3 \r");
    music_clear();
    qbrt_realtime(true);
}

TEST(sound_out_of_range)
{
    static const char *const lines[] = {"10 SOUND 20, 1", nullptr};

    CHECK(unit_run(S, lines) == E_USR);
}

/* ON PLAY(2) runs once, as the queue falls below 2 notes; PLAY ON, OFF, STOP compile. Each note
   lasts a tick: shorter ones drain the queue between two polls, and no fall is seen. */
TEST(on_play_fires_as_the_queue_drains)
{
    static const char *const lines[] = {
        "10 ON PLAY(2) GOSUB 100: PLAY OFF: PLAY STOP: PLAY ON",
        "20 PLAY \"MB T255 L16 CDEFG\"",
        "30 IF PLAY(0) THEN 30",
        "40 PRINT n: END",
        "100 n = n + 1: RETURN",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 1 \r");
}
