/* Songs (llrm's song.c): instrument, music, music stop, note and beat(), heard through the headless device's log of
   voices, each with the part of a tick it came at. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "session.h"
#include "qbrt/llrm/qb/dev/voice.h"
#include "qbrt/qbrt.h"
#include "unit.h"

/* llrm's headless device (dev/headless/headless.h) and music queue (music.h). */
typedef struct {
    unsigned voice;
    DevVoice v[DEV_LAYERS];
    long tick;
} HeadlessVoice;
size_t headless_voice_count(void);
HeadlessVoice headless_voice(size_t index);
void headless_voice_forget(void);
void headless_run(long hundredths);
void music_fine_tick(void);
void music_clear(void);

enum { HEARD = 1024, TOP = DEV_VOICES - 1 };

typedef struct {
    unsigned part, voice;
    DevVoice v[DEV_LAYERS];
} Heard;
static Heard heard[HEARD];
static unsigned heard_count;

/* A tick's parts at T120: music.h's parts a minute over a minute's 96ths of a quarter. */
static const double PER_TICK = 1092390.0 * 16 / 1000 / (96 * 120);

/* The event record, then `rest`: a program's lines. */
#define EVENT_TYPE "10 TYPE Event", "11 at AS INTEGER", "12 length AS INTEGER", "13 kind AS INTEGER", \
                   "14 pitch AS INTEGER", "15 loud AS INTEGER", "16 END TYPE"

/* `lines` run, then `parts` parts of a tick: each voice the song sent, with its part, from 1. */
static void listen(Session *const S, const char *const *const lines, const unsigned parts)
{
    qbrt_realtime(false);
    music_clear();
    headless_voice_forget();
    CHECK(unit_run(S, lines) == ST_OK);
    size_t seen = headless_voice_count();
    heard_count = 0;
    for (unsigned p = 1; p <= parts; p++) {
        music_fine_tick();
        for (; seen < headless_voice_count(); seen++)
            if (heard_count < HEARD) {
                const HeadlessVoice h = headless_voice(seen);
                heard[heard_count].part = p;
                heard[heard_count].voice = h.voice;
                memcpy(heard[heard_count].v, h.v, sizeof h.v);
                heard_count++;
            }
    }
    music_clear();
    headless_run(20); /* the clock's tick takes the queues back */
    qbrt_realtime(true);
}

/* The notes struck: the voices heard gated with a new trigger, in order. */
static unsigned struck(Heard *const out, const unsigned most)
{
    unsigned n = 0, trigger[DEV_VOICES] = {0};

    for (unsigned i = 0; i < heard_count && n < most; i++) {
        const Heard *const h = &heard[i];

        if (h->v[0].gate && h->v[0].trigger != trigger[h->voice]) {
            trigger[h->voice] = h->v[0].trigger;
            out[n++] = *h;
        }
    }
    return n;
}

/* instrument sets a sound by PLAY's commands, its layers too; note strikes it at the next part, on the top voice,
   and lets it go its length later. */
TEST(song_note_strikes_an_instrument)
{
    static const char *const lines[] = {"10 instrument 1, \"W4 Z1,0 V12 J1 W3 V14\"", "20 note 1, 37, 24, 50",
                                        nullptr};

    listen(S, lines, 60);
    CHECK(heard_count == 2);
    const Heard *const on = &heard[0], *const off = &heard[1];
    CHECK(on->part == 1 && on->voice == TOP && on->v[0].gate && on->v[0].hertz == 262);
    CHECK(on->v[0].wave == DEV_FM && on->v[0].layers == 1 && on->v[1].wave == DEV_NOISE);
    CHECK(on->v[0].volume == 6 && on->v[1].volume == 7);
    CHECK(off->voice == TOP && !off->v[0].gate && off->part == 1 + (unsigned)lround(24 * PER_TICK));
}

/* 128 sixteenths: each starts where its tick falls from the song's start, so the last is no later than the first. */
TEST(song_keeps_one_clock)
{
    static const char *const lines[] = {
        EVENT_TYPE,
        "20 instrument 1, \"W1 H0,0,15,0\"",
        "30 DIM ev(127) AS Event",
        "40 FOR i = 0 TO 127: ev(i).at = i * 24: ev(i).length = 12: ev(i).kind = 1: ev(i).pitch = 37",
        "50 ev(i).loud = 100: NEXT",
        "60 music ev()",
        nullptr};
    static Heard on[128];

    listen(S, lines, (unsigned)(128 * 24 * PER_TICK) + 10);
    CHECK(struck(on, 128) == 128);
    unsigned late = 0;
    for (unsigned k = 0; k < 128; k++)
        late += on[k].part != 1 + (unsigned)lround(k * 24 * PER_TICK);
    CHECK(!late);
}

/* Two sounds on one step each take a voice; a chord's three notes, three more; a sound's layers come in one. */
TEST(song_gives_each_note_a_voice)
{
    static const char *const lines[] = {
        EVENT_TYPE,
        "20 instrument 1, \"W1 H0,0,15,4\": instrument 2, \"W3 H0,2,0,2 J1,200 W1\"",
        "30 DIM ev(4) AS Event",
        "40 FOR i = 0 TO 4: READ ev(i).at, ev(i).length, ev(i).kind, ev(i).pitch, ev(i).loud: NEXT",
        "50 DATA 0,48,1,37,100, 0,24,2,49,80, 48,48,1,37,100, 48,48,1,41,100, 48,48,1,44,100",
        "60 music ev()",
        nullptr};
    Heard on[5];

    listen(S, lines, 200);
    CHECK(struck(on, 5) == 5);
    CHECK(on[0].part == 1 && on[1].part == 1 && on[0].voice != on[1].voice);
    CHECK(on[1].v[0].wave == DEV_NOISE && on[1].v[0].layers == 1 && on[1].v[1].wave == DEV_TRIANGLE);
    const unsigned chord = 1 + (unsigned)lround(48 * PER_TICK);
    CHECK(on[2].part == chord && on[3].part == chord && on[4].part == chord);
    CHECK(on[2].voice != on[3].voice && on[3].voice != on[4].voice && on[2].voice != on[4].voice);
    CHECK(on[2].v[0].hertz != on[3].v[0].hertz && on[3].v[0].hertz != on[4].v[0].hertz);
}

/* At its end the song goes back to the tick it loops from, and on round again. */
TEST(song_loops_from_its_tick)
{
    static const char *const lines[] = {
        EVENT_TYPE,
        "20 instrument 1, \"W1\"",
        "30 DIM ev(1) AS Event",
        "40 FOR i = 0 TO 1: READ ev(i).at, ev(i).length, ev(i).kind, ev(i).pitch, ev(i).loud: NEXT",
        "50 DATA 0,24,1,37,100, 48,24,1,44,100",
        "60 music ev(), 48",
        nullptr};
    Heard on[5];
    const unsigned from = (unsigned)lround(48 * PER_TICK), end = (unsigned)lround(72 * PER_TICK);

    listen(S, lines, 1 + end + 3 * (end - from));
    CHECK(struck(on, 5) == 5);
    CHECK(on[0].part == 1 && on[0].v[0].hertz == 262 && on[1].part == 1 + from && on[1].v[0].hertz == 392);
    for (unsigned k = 2; k < 5; k++)
        CHECK(on[k].part == on[k - 1].part + end - from && on[k].v[0].hertz == 392);
}

/* A tempo is an event: T240's sixteenths are half T120's. */
TEST(song_tempo_is_an_event)
{
    static const char *const lines[] = {
        EVENT_TYPE,
        "20 instrument 1, \"W1\"",
        "30 DIM ev(3) AS Event",
        "40 FOR i = 0 TO 3: READ ev(i).at, ev(i).length, ev(i).kind, ev(i).pitch, ev(i).loud: NEXT",
        "50 DATA 96,24,1,37,100, 0,0,0,120,0, 96,0,0,240,0, 192,24,1,37,100",
        "60 music ev()",
        nullptr};
    Heard on[2];

    listen(S, lines, 300);
    CHECK(struck(on, 2) == 2);
    CHECK(on[0].part == 1 + (unsigned)lround(96 * PER_TICK));
    CHECK(on[1].part == 1 + (unsigned)lround(96 * PER_TICK + 96 * PER_TICK / 2));
}

/* beat() counts the song's quarters, on through its loop, and is 0 once music stop ends it; a fade lowers the
   notes sounding, then lets them go. */
TEST(song_beat_counts_and_stop_fades)
{
    static const char *const lines[] = {
        EVENT_TYPE,
        "20 instrument 1, \"W1 H0,0,15,0 V12\"",
        "30 DIM ev(0) AS Event: ev(0).length = 96: ev(0).kind = 1: ev(0).pitch = 37: ev(0).loud = 100",
        "40 music ev(), 0",
        "50 DO: LOOP UNTIL beat() >= 2.5",
        "60 PRINT INT(beat()); : music stop, 96",
        "70 FOR i = 1 TO 5000: IF beat() = 0 THEN EXIT FOR",
        "80 NEXT: PRINT i < 5000",
        nullptr};

    qbrt_realtime(false);
    music_clear();
    headless_voice_forget();
    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 2 -1 \r");
    unsigned volume = 99, falls = 0;
    bool let_go = false;
    for (size_t i = 0; i < headless_voice_count(); i++) {
        const HeadlessVoice h = headless_voice(i);

        if (h.voice != TOP)
            continue;
        if (h.v[0].gate && h.v[0].volume < volume)
            falls++;
        volume = h.v[0].gate ? h.v[0].volume : volume;
        let_go = !h.v[0].gate;
    }
    CHECK(falls > 3 && let_go);
    music_clear();
    headless_run(20); /* the clock's tick takes the queues back */
    qbrt_realtime(true);
}

/* A program of QBasic's that names a variable, an array or a procedure instrument, music, note or beat runs as
   before: the words are ours only where it could not have written them. */
TEST(song_words_stay_the_programs_names)
{
    static const char *const names[] = {
        "10 DECLARE SUB music (a, b)",
        "20 note = 5: instrument = 1: beat = 2",
        "30 music note, instrument",
        "40 PRINT note; instrument; beat",
        "50 SUB music (a, b)",
        "60 PRINT a + b;",
        "70 END SUB",
        nullptr};
    static const char *const arrays[] = {"10 DIM note(3), beat(2): note(2) = 4: beat(1) = 3", "20 PRINT note(2); beat(1)",
                                         nullptr};
    static const char *const function[] = {"10 PRINT beat()", "20 FUNCTION beat!", "30 beat! = 7",
                                           "40 END FUNCTION", nullptr};

    CHECK(unit_run(S, names) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 6  5  1  2 \r");
    CHECK(unit_run(S, arrays) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 4  3 \r");
    CHECK(unit_run(S, function) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 7 \r");
}

/* A record not an event's is a Type mismatch; an instrument not set, a pitch past PLAY's, an instrument past 64 or
   one that plays a note: Illegal function call. */
TEST(song_refuses_what_it_cannot_play)
{
    static const char *const wrong[][3] = {
        {"10 DIM a(3): music a()", nullptr},
        {"10 note 3, 37, 24, 100", nullptr},
        {"10 instrument 1, \"W1\": note 1, 85, 24, 100", nullptr},
        {"10 instrument 65, \"W1\"", nullptr},
        {"10 instrument 1, \"W1 C\"", nullptr},
    };

    for (size_t i = 0; i < sizeof wrong / sizeof wrong[0]; i++)
        CHECK(unit_run(S, wrong[i]) == (i ? E_USR : E_TM));
}
