/* Songs (song.h). A song's notes are kept in the order they start, their times in parts of a tick worked out from
   the song's start when it is given, so a long song does not drift. The music's tick (music.c) plays them: nothing is
   fed while a song plays. Each note takes a voice of its own, from the top down, where PLAY's take theirs from the
   bottom: a voice PLAY has nothing on, whose last note's release is done, or else the one whose release ends first.
   With every voice PLAY's, a note is not played. */
#include <string.h>

#include "music.h"
#include "song.h"
#include "nhstutil.h"

enum {
    TEMPI = 64,     /* changes of tempo a song holds */
    STRIKES = 16,   /* notes struck by `note`, waiting for the tick */
    FADE_EVERY = 4, /* parts of a tick between a fade's steps */
    DEFAULT_TEMPO = 120,
    LOWEST_TEMPO = 32,
    HIGHEST_TEMPO = 255,
    HIGHEST_NOTE = 84,
    FULL = 100, /* loud's, and a volume's hundredths */
    RECORD = 20 /* an event's five integers */
};

/* SID's release times, ms (plat/synth's decay_ms): a voice is free once its note's release is done */
static const unsigned short release_ms[16] = {6,   24,  48,   72,   114,  168,  204,   240,
                                              300, 750, 1500, 2400, 3000, 9000, 15000, 24000};

typedef struct Note {
    unsigned long on, off; /* parts from the song's start */
    unsigned char sound, pitch, loud;
} Note;

typedef struct Tempo {
    long tick;
    double part, per_tick; /* where it starts, in parts, and a tick's parts */
} Tempo;

/* A voice as the song left it. */
typedef struct Held {
    unsigned long off, free; /* the clock's part its note lets go at, and when its release is done */
    byte gated, ours, loud, trigger;
    DevVoice sound[DEV_LAYERS]; /* its instrument's, at the note's pitch */
} Held;

typedef struct Strike {
    unsigned char sound, pitch, loud;
    unsigned long length; /* parts */
} Strike;

static DevVoice instruments[SONG_INSTRUMENTS + 1][DEV_LAYERS];
static unsigned long tails[SONG_INSTRUMENTS + 1]; /* an instrument's longest release, parts; 0 not defined */

static Note notes[SONG_NOTES];
static Tempo tempi[TEMPI];
static unsigned count, tempo_count, loop_next;
static unsigned long end, loop_at;  /* parts: the song's end, and where it goes back to */
static long end_tick, loop_tick;    /* the same in ticks; loop_tick < 0, none */
static volatile unsigned next;      /* the next note to start */
static volatile unsigned long at;   /* parts into the song */
static volatile unsigned long passes;
static volatile byte live, looping;
static volatile unsigned long fade_left, fade_total;

static Held held[DEV_VOICES];
static Strike strikes[STRIKES];
static volatile unsigned strike_head, strike_tail;
static volatile unsigned long clock; /* parts the song's tick has run */
static volatile byte awake;          /* a song, a strike or a note of ours: the tick has work */

#ifndef QB_IDLE
#define QB_IDLE() ((void)0)
#endif

static void illegal(void)
{
    qb_error(BE_ILLFUN);
}

/* Parts of a tick a minute. */
static double parts_a_minute(void)
{
    return (double)TICKS_PER_MINUTE * MUSIC_FINE / 1000.0;
}

/* A tick's parts at `tempo` quarters a minute. */
static double per_tick(long tempo)
{
    return parts_a_minute() / (double)(SONG_BEAT * tempo);
}

/* The tempo in force at tick `tick`. */
static const Tempo *tempo_at(long tick)
{
    unsigned k = 0;

    while (k + 1 < tempo_count && tempi[k + 1].tick <= tick)
        k++;
    return &tempi[k];
}

/* Where tick `tick` falls, in parts from the song's start. */
static unsigned long part_of(long tick)
{
    const Tempo *const t = tempo_at(tick);

    return (unsigned long)(t->part + (double)(tick - t->tick) * t->per_tick + 0.5);
}

/* The tick part `part` falls on. */
static double tick_of(unsigned long part)
{
    unsigned k = 0;

    while (k + 1 < tempo_count && tempi[k + 1].part <= (double)part)
        k++;
    return (double)tempi[k].tick + ((double)part - tempi[k].part) / tempi[k].per_tick;
}

/* The voice's sound to the synthesizer, at its note's loudness and the fade's. */
static void send(unsigned voice)
{
    const Held *const h = &held[voice];
    DevVoice v[DEV_LAYERS];
    const unsigned layers = 1u + h->sound[0].layers;

    memcpy(v, h->sound, layers * sizeof v[0]);
    for (unsigned k = 0; k < layers; k++) {
        unsigned long level = ((unsigned long)v[k].volume * FULL + v[k].loud) * h->loud / FULL;

        if (fade_total)
            level = level * fade_left / fade_total;
        v[k].volume = (unsigned char)(level / FULL);
        v[k].loud = (unsigned char)(level % FULL);
    }
    dev_voice(voice, v);
}

static void let_go(unsigned voice)
{
    Held *const h = &held[voice];

    h->gated = 0;
    h->sound[0].gate = 0;
    send(voice);
}

/* A voice for a note: see the top of the file. DEV_VOICES none. */
static unsigned pick(void)
{
    unsigned best = DEV_VOICES;

    for (int i = DEV_VOICES - 1; i >= 0; i--) {
        const Held *const h = &held[i];

        if (!music_idle((unsigned)i))
            continue;
        if (!h->ours || clock >= h->free)
            return (unsigned)i;
        if (best == DEV_VOICES || h->free < held[best].free)
            best = (unsigned)i;
    }
    return best;
}

/* Sound `sound`'s note `pitch` struck now, `length` parts long, `loud` hundredths of its level. */
static void start(unsigned sound, unsigned pitch, unsigned long length, unsigned loud)
{
    const unsigned voice = pick();

    if (voice == DEV_VOICES || !tails[sound])
        return;
    Held *const h = &held[voice];

    memcpy(h->sound, instruments[sound], sizeof h->sound);
    h->sound[0].hertz = play_hertz((int)pitch);
    h->sound[0].gate = 1;
    h->sound[0].trigger = ++h->trigger;
    h->loud = (byte)loud;
    h->off = clock + (length ? length : 1);
    h->free = h->off + tails[sound];
    h->gated = h->ours = 1;
    send(voice);
}

/* The song ends: its notes let go. */
static void stop_now(void)
{
    live = 0;
    fade_left = fade_total = 0;
    for (unsigned i = 0; i < DEV_VOICES; i++)
        if (held[i].ours && held[i].gated)
            let_go(i);
}

void song_tick(void)
{
    if (!awake)
        return;
    clock++;
    byte gated = 0;

    for (unsigned i = 0; i < DEV_VOICES; i++)
        if (held[i].gated && clock >= held[i].off)
            let_go(i);
    while (strike_tail != strike_head) {
        const Strike *const s = &strikes[strike_tail];

        start(s->sound, s->pitch, s->length, s->loud);
        strike_tail = (strike_tail + 1) % STRIKES;
    }
    if (live) {
        while (next < count && notes[next].on <= at) {
            const Note *const n = &notes[next++];

            start(n->sound, n->pitch, n->off - n->on, n->loud);
        }
        if (++at >= end) {
            if (looping) {
                at = loop_at;
                next = loop_next;
                passes++;
            } else
                live = 0;
        }
    }
    if (fade_total) {
        fade_left--;
        if (!fade_left)
            stop_now();
        else if (fade_left % FADE_EVERY == 0)
            for (unsigned i = 0; i < DEV_VOICES; i++)
                if (held[i].ours && held[i].gated)
                    send(i);
    }
    for (unsigned i = 0; i < DEV_VOICES; i++)
        gated |= held[i].gated;
    if (!live && !gated && strike_head == strike_tail)
        awake = 0;
}

void song_taken(unsigned voice)
{
    held[voice].ours = held[voice].gated = 0;
}

void song_clear(void)
{
    static const DevVoice silence = {0};

    live = awake = 0;
    fade_left = fade_total = 0;
    strike_tail = strike_head;
    for (unsigned i = 0; i < DEV_VOICES; i++) {
        if (held[i].ours)
            dev_voice(i, &silence);
        held[i].ours = held[i].gated = 0;
    }
}

void song_instrument(int n, SD *commands)
{
    DevVoice sound[DEV_LAYERS];
    unsigned long longest = 0;

    if (n < 1 || n > SONG_INSTRUMENTS)
        illegal();
    play_sound(commands, sound);
    for (unsigned k = 0; k <= sound[0].layers; k++) {
        const unsigned long ms = release_ms[sound[k].envelope & 15];

        longest = ms > longest ? ms : longest;
    }
    dev_tick_hold();
    memcpy(instruments[n], sound, sizeof sound);
    tails[n] = 1 + (unsigned long)((double)longest * parts_a_minute() / 60000.0);
    dev_tick_release();
}

/* An event's integer `k`. */
static long field(const unsigned char *event, unsigned k)
{
    int value;

    memcpy(&value, event + 4 * k, sizeof value);
    return value;
}

void song_play(const unsigned char *events, unsigned n, unsigned size, long from)
{
    dev_tick_hold();
    stop_now();
    dev_tick_release();

    /* the tempi first, in order of their ticks: every note's time depends on them */
    tempo_count = 1;
    tempi[0].tick = 0;
    tempi[0].per_tick = per_tick(DEFAULT_TEMPO);
    end_tick = 0;
    for (unsigned i = 0; i < n; i++) {
        const unsigned char *const e = events + (unsigned long)i * size;
        const long tick = field(e, 0), length = field(e, 1), sound = field(e, 2), pitch = field(e, 3);

        if (tick < 0 || length < 0 || sound < 0 || sound > SONG_INSTRUMENTS)
            illegal();
        end_tick = tick + length > end_tick ? tick + length : end_tick;
        if (sound)
            continue;
        if (pitch < LOWEST_TEMPO || pitch > HIGHEST_TEMPO)
            illegal();
        unsigned k = tempo_count;

        if (!tick)
            k = 0;
        else if (tempo_count == TEMPI)
            illegal();
        else {
            while (k > 1 && tempi[k - 1].tick > tick) {
                tempi[k] = tempi[k - 1];
                k--;
            }
            tempo_count++;
        }
        tempi[k].tick = tick;
        tempi[k].per_tick = per_tick(pitch);
    }
    tempi[0].part = 0;
    for (unsigned k = 1; k < tempo_count; k++)
        tempi[k].part = tempi[k - 1].part + (double)(tempi[k].tick - tempi[k - 1].tick) * tempi[k - 1].per_tick;

    count = 0;
    for (unsigned i = 0; i < n; i++) {
        const unsigned char *const e = events + (unsigned long)i * size;
        const long tick = field(e, 0), length = field(e, 1), sound = field(e, 2), pitch = field(e, 3),
                   loud = field(e, 4);

        if (!sound)
            continue;
        if (pitch < 0 || pitch > HIGHEST_NOTE || loud < 0 || loud > FULL || !tails[sound])
            illegal();
        if (!pitch)
            continue; /* a rest: the song's end at least as late as it */
        if (count == SONG_NOTES)
            illegal();
        Note *const note = &notes[count++];

        note->on = part_of(tick);
        note->off = part_of(tick + length);
        if (note->off <= note->on)
            note->off = note->on + 1;
        note->sound = (unsigned char)sound;
        note->pitch = (unsigned char)pitch;
        note->loud = (unsigned char)loud;
    }
    /* in the order they start: a shell sort, no library's */
    for (unsigned gap = count / 2; gap; gap /= 2)
        for (unsigned i = gap; i < count; i++) {
            const Note moving = notes[i];
            unsigned j = i;

            for (; j >= gap && notes[j - gap].on > moving.on; j -= gap)
                notes[j] = notes[j - gap];
            notes[j] = moving;
        }

    end = part_of(end_tick);
    end = end ? end : 1;
    looping = from >= 0;
    loop_tick = from;
    loop_at = end;
    loop_next = count;
    if (looping) {
        if (from >= end_tick)
            illegal();
        loop_at = part_of(from);
        for (loop_next = 0; loop_next < count && notes[loop_next].on < loop_at; loop_next++)
            ;
    }
    dev_tick_hold();
    at = passes = 0;
    next = 0;
    live = awake = 1;
    dev_tick_release();
    music_start();
}

void song_stop(long ticks)
{
    if (ticks < 0)
        illegal();
    const unsigned long parts = (unsigned long)((double)ticks * tempo_at((long)tick_of(at))->per_tick + 0.5);

    dev_tick_hold();
    if (!ticks || !live)
        stop_now();
    else
        fade_left = fade_total = parts ? parts : 1;
    dev_tick_release();
}

void song_note(int sound, int pitch, long length, int loud)
{
    if (sound < 1 || sound > SONG_INSTRUMENTS || !tails[sound] || pitch < 0 || pitch > HIGHEST_NOTE || length < 0 ||
        loud < 0 || loud > FULL)
        illegal();
    if (!pitch)
        return;
    const double now = live ? tempo_at((long)tick_of(at))->per_tick : per_tick(DEFAULT_TEMPO);

    music_start();
    while ((strike_head + 1) % STRIKES == strike_tail)
        QB_IDLE();
    Strike *const s = &strikes[strike_head];

    s->sound = (unsigned char)sound;
    s->pitch = (unsigned char)pitch;
    s->loud = (unsigned char)loud;
    s->length = (unsigned long)((double)length * now + 0.5);
    strike_head = (strike_head + 1) % STRIKES;
    awake = 1;
}

float song_beat(void)
{
    unsigned long part, pass;

    do {
        pass = passes;
        part = at;
    } while (pass != passes);
    if (!live)
        return 0;
    return (float)((tick_of(part) + (double)pass * (double)(end_tick - loop_tick)) / SONG_BEAT);
}
