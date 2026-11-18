/* The synthesizer (plat/synth) rendered offline, and PLAY's voices on llrm's queues (play.c) through the headless
   device's logs. */
#include <stdlib.h>

#include "session.h"
#include "plat/synth/synth.h"
#include "qbrt/qbrt.h"
#include "unit.h"

/* llrm's headless device (dev/headless/headless.h). */
typedef struct {
    unsigned hertz;
    long clock, tick;
} HeadlessTone;
typedef struct {
    unsigned voice;
    DevVoice v;
    long tick;
} HeadlessVoice;
size_t headless_tone_count(void);
HeadlessTone headless_tone(size_t index);
size_t headless_voice_count(void);
HeadlessVoice headless_voice(size_t index);
void headless_run(long hundredths);

enum { RATE = 44100, TICK_SAMPLES = RATE * 10 / 182 };

static int16_t buf[RATE];

/* SID's first voice as PLAY's defaults set it, with `wave` and `envelope`. */
static DevVoice note(const unsigned hertz, const unsigned wave, const unsigned envelope)
{
    return (DevVoice){.hertz = hertz, .width = 2048, .envelope = (unsigned short)envelope, .cutoff = 1000,
                      .wave = (unsigned char)wave, .volume = 15, .gate = 1, .trigger = 1};
}

/* `v` from silence, n samples of it in buf. */
static void sound(Synth *const s, const DevVoice v, const int n)
{
    synth_init(s, RATE);
    synth_voice(s, 0, &v);
    synth_render(s, buf, n);
}

/* Sign changes from below to above zero in n samples. */
static int rises(const int16_t *const s, const int n)
{
    int count = 0;

    for (int i = 1; i < n; i++)
        count += s[i - 1] < 0 && s[i] >= 0;
    return count;
}

static int peak(const int16_t *const s, const int n)
{
    int top = 0;

    for (int i = 0; i < n; i++)
        top = s[i] > top ? s[i] : -s[i] > top ? -s[i] : top;
    return top;
}

/* A second of each wave at 440 Hz rises 440 times; noise far more often. */
TEST(synth_waves_have_their_pitch)
{
    Synth s;

    for (unsigned w = DEV_PULSE; w <= DEV_SAW; w++) {
        sound(&s, note(440, w, 0x00F0), RATE);
        CHECK(rises(buf, RATE) >= 439 && rises(buf, RATE) <= 440); /* a pulse starts high */
    }
    sound(&s, note(440, DEV_NOISE, 0x00F0), RATE);
    CHECK(rises(buf, RATE) > 1500);
}

/* A pulse of width 1024 (a quarter) is high a quarter of the time; a triangle takes every value between its peaks; a
   saw climbs, then falls at once. */
TEST(synth_waves_have_their_shape)
{
    Synth s;
    DevVoice v = note(441, DEV_PULSE, 0x00F0);
    v.width = 1024;
    sound(&s, v, RATE);
    int high = 0;
    for (int i = RATE / 10; i < RATE; i++)
        high += buf[i] > 0;
    CHECK(high > (RATE - RATE / 10) * 24 / 100 && high < (RATE - RATE / 10) * 26 / 100);

    sound(&s, note(441, DEV_TRIANGLE, 0x00F0), RATE);
    int turns = 0;
    for (int i = RATE / 10 + 1; i < RATE - 1; i++)
        turns += (buf[i] - buf[i - 1] > 0) != (buf[i + 1] - buf[i] > 0);
    CHECK(turns >= 2 * 441 * 9 / 10 - 2 && turns <= 2 * 441 * 9 / 10 + 2);

    sound(&s, note(441, DEV_SAW, 0x00F0), RATE);
    int drops = 0;
    for (int i = RATE / 10 + 1; i < RATE; i++)
        drops += buf[i] < buf[i - 1];
    CHECK(drops >= 441 * 9 / 10 - 1 && drops <= 441 * 9 / 10 + 1);
}

/* FM (W4): with no depth a sine, which turns twice a period and has no step a saw's size; a modulator at three times
   the note bends it into more turns, the period the same; and the bend fades with the envelope, so a decayed note's
   tail is the sine again. */
TEST(synth_fm_bends_a_sine)
{
    Synth s;
    DevVoice v = note(441, DEV_FM, 0x00F0);
    v.ratio = 3;
    sound(&s, v, RATE);
    int turns = 0, step = 0;
    for (int i = RATE / 10 + 1; i < RATE - 1; i++) {
        turns += (buf[i] - buf[i - 1] > 0) != (buf[i + 1] - buf[i] > 0);
        step = abs(buf[i] - buf[i - 1]) > step ? abs(buf[i] - buf[i - 1]) : step;
    }
    CHECK(peak(buf, RATE) > 5900 && peak(buf, RATE) <= 6000);
    CHECK(turns >= 2 * 441 * 9 / 10 - 2 && turns <= 2 * 441 * 9 / 10 + 2);
    CHECK(step < 6000 / 10);

    v.depth = 10;
    sound(&s, v, RATE);
    turns = 0;
    for (int i = RATE / 10 + 1; i < RATE - 1; i++)
        turns += (buf[i] - buf[i - 1] > 0) != (buf[i + 1] - buf[i] > 0);
    CHECK(turns > 4 * 441 * 9 / 10);
    int apart = 0; /* 441 Hz: a period is 100 samples */
    for (int i = RATE / 10; i < RATE - 100; i++)
        apart = abs(buf[i] - buf[i + 100]) > apart ? abs(buf[i] - buf[i + 100]) : apart;
    CHECK(apart < 6000 / 20);

    v.envelope = 0x0680; /* decays in 204 ms to about half */
    sound(&s, v, RATE);
    int early = 0, late = 0;
    for (int i = 1; i < RATE / 50 - 1; i++)
        early += (buf[i] - buf[i - 1] > 0) != (buf[i + 1] - buf[i] > 0);
    for (int i = RATE / 2 + 1; i < RATE / 2 + RATE / 50 - 1; i++)
        late += (buf[i] - buf[i - 1] > 0) != (buf[i + 1] - buf[i] > 0);
    CHECK(early > late);
}

/* H8,0,8,4: 100 ms of attack to full, a decay to about half, held, then a release to silence. */
TEST(synth_envelope_attacks_sustains_releases)
{
    Synth s;
    DevVoice v = note(1000, DEV_SAW, 0x8084);
    sound(&s, v, RATE / 20); /* 50 ms */
    const int half_way = peak(buf + RATE / 20 - RATE / 100, RATE / 100);
    synth_render(&s, buf, RATE / 20);
    const int full = peak(buf + RATE / 20 - RATE / 100, RATE / 100);
    synth_render(&s, buf, RATE / 2);
    const int held = peak(buf + RATE / 2 - RATE / 100, RATE / 100);
    CHECK(full > 5900 && half_way > full * 4 / 10 && half_way < full * 6 / 10);
    CHECK(held > full * 45 / 100 && held < full * 6 / 10);

    v.gate = 0;
    synth_voice(&s, 0, &v);
    synth_render(&s, buf, RATE / 2);
    CHECK(peak(buf + RATE / 2 - RATE / 100, RATE / 100) == 0);
}

/* A low-pass at 300 Hz leaves little of a 3 kHz saw; a high-pass leaves most. */
TEST(synth_filter_passes_its_band)
{
    Synth s;
    int got[2];

    for (int f = 0; f < 2; f++) {
        DevVoice v = note(3000, DEV_SAW, 0x00F0);
        v.filter = f ? DEV_HIGH_PASS : DEV_LOW_PASS;
        v.cutoff = 300;
        sound(&s, v, RATE / 10);
        got[f] = peak(buf + RATE / 20, RATE / 20);
    }
    CHECK(got[0] < 1000 && got[1] > 4000);
}

/* The page's AudioWorklet (synth.wasm) renders filtered noise to this hash; the Mac and DOS must too. A float filter
   once differed by arm64's fused multiply-adds. */
TEST(synth_renders_as_the_page_does)
{
    Synth s;
    DevVoice v = note(3000, DEV_NOISE, 0x5A93);
    v.width = 1000;
    v.cutoff = 900;
    v.volume = 11;
    v.filter = DEV_LOW_PASS;
    v.resonance = 12;
    sound(&s, v, RATE);
    uint32_t hash = 0;
    for (int i = 0; i < RATE; i++)
        hash = hash * 31 + (uint32_t)buf[i];
    CHECK(hash == 2313499851u);
}

/* Every wave under every filter, six voices at once with the speaker, released halfway, one cut at three quarters,
   rendered in uneven pieces: the samples as they were before rendering went a voice a block at a time. */
TEST(synth_six_voices_render_as_before)
{
    Synth s;
    synth_init(&s, 15873);
    synth_tone(&s, 440);
    for (unsigned k = 0; k < DEV_VOICES; k++) {
        DevVoice v = note(110 + 97 * k, k % 4, 0x1A63 + 0x1111 * k);
        v.filter = (unsigned char)(k % 4 == 0 ? 3 - k / 4 : k % 4);
        v.cutoff = (unsigned short)(400 + 700 * k);
        v.resonance = (unsigned char)(3 * k);
        v.volume = (unsigned char)(15 - k);
        synth_voice(&s, k, &v);
    }
    uint32_t hash = 0;
    for (int at = 0, piece = 1; at < RATE; at += piece, piece = piece * 7 % 601 + 1) {
        const int n = piece < RATE - at ? piece : RATE - at;
        if (at >= RATE / 2 && at - piece < RATE / 2) {
            synth_tone(&s, 0);
            for (unsigned k = 0; k < DEV_VOICES; k += 2) {
                DevVoice v = s.osc[k].set;
                v.gate = 0;
                synth_voice(&s, k, &v);
            }
        }
        if (at >= RATE * 3 / 4 && at - piece < RATE * 3 / 4) { /* one cut off at once */
            DevVoice v = s.osc[1].set;
            v.hertz = 0;
            synth_voice(&s, 1, &v);
        }
        synth_render(&s, buf + at, n);
    }
    for (int i = 0; i < RATE; i++)
        hash = hash * 31 + (uint32_t)buf[i];
    CHECK(hash == 2531076252u);
}

/* QB's speaker as plat/sdl/audio.c made it before the synthesizer: the reference. */
typedef struct {
    uint32_t hertz, phase;
    int32_t gain;
} OldSpeaker;

static void old_speaker(OldSpeaker *const o, const uint32_t want, int16_t *const out, const int n)
{
    enum { FADE = RATE / 500, LOUD = 4000 };

    for (int i = 0; i < n; i++) {
        if (want != o->hertz && o->gain == 0) {
            o->hertz = want;
            o->phase = 0;
        }
        o->gain += want == o->hertz && o->hertz ? o->gain < FADE : -(o->gain > 0);
        o->phase = (o->phase + o->hertz) % RATE;
        out[i] = (int16_t)((o->phase < RATE / 2 ? LOUD : -LOUD) * o->gain / FADE);
    }
}

/* One string of QB's commands, as before the voices: the same tones at the same ticks (the log of the speaker before
   them), no voice, and the same samples as the old speaker's. */
TEST(play_of_one_string_sounds_as_before)
{
    static const char *const lines[] = {
        "10 PLAY \"T180 L8 O3 C D# E- MS F G. ML A B > C P8 MN N40 MB L16 GFE\": SOUND 300, 3", nullptr};
    static const struct {
        unsigned hertz;
        long tick;
    } before[] = {{262, 0}, {0, 3},  {311, 3},  {0, 5},  {311, 5},  {0, 8},  {349, 9},  {0, 11}, {392, 11},
                  {0, 15},  {440, 17}, {0, 20}, {494, 20}, {0, 23}, {524, 23}, {0, 26}, {0, 29}, {311, 29},
                  {0, 32},  {784, 32}, {0, 33}, {698, 33}, {0, 34}, {660, 34}, {0, 35}};
    enum { TONES = sizeof before / sizeof before[0] };

    qbrt_realtime(false);
    music_clear(); /* an earlier test's music */
    const size_t first = headless_tone_count(), voices = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    headless_run(200);
    CHECK(headless_voice_count() == voices);
    CHECK(headless_tone_count() - first == TONES);
    for (size_t i = 0; i < TONES && first + i < headless_tone_count(); i++) {
        const HeadlessTone t = headless_tone(first + i);
        CHECK(t.hertz == before[i].hertz && t.tick - headless_tone(first).tick == before[i].tick);
    }
    qbrt_realtime(true);

    /* the ticks' tones rendered: the last tone set in each tick sounds through it */
    static int16_t old[TICK_SAMPLES], now[TICK_SAMPLES];
    OldSpeaker o = {0};
    Synth s;
    synth_init(&s, RATE);
    uint32_t want = 0;
    for (long tick = 0, at = 0; tick <= before[TONES - 1].tick + 2; tick++) {
        for (; at < TONES && before[at].tick == tick; at++)
            want = before[at].hertz;
        synth_tone(&s, want);
        old_speaker(&o, want, old, TICK_SAMPLES);
        synth_render(&s, now, TICK_SAMPLES);
        CHECK(!memcmp(old, now, sizeof old));
    }
}

/* PLAY a$, b$, c$: each voice's notes start together on the synthesizer, at their own pitch, and end gated off. */
TEST(play_of_three_strings_plays_three_voices)
{
    static const char *const lines[] = {"10 PLAY \"MF T120 L4 O3 C E\", \"O3 E G\", \"O3 G > C\"", nullptr};

    qbrt_realtime(false);
    music_clear(); /* an earlier test's music */
    const size_t tones = headless_tone_count(), first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    headless_run(200);
    qbrt_realtime(true);

    unsigned on[3][2] = {{0}}, ons[3] = {0}, offs[3] = {0};
    long start[3][2] = {{0}};
    for (size_t i = first; i < headless_voice_count(); i++) {
        const HeadlessVoice h = headless_voice(i);
        CHECK(h.voice < 3);
        if (h.v.gate && ons[h.voice] < 2) {
            start[h.voice][ons[h.voice]] = h.tick;
            on[h.voice][ons[h.voice]++] = h.v.hertz;
        } else if (!h.v.gate && h.v.hertz)
            offs[h.voice]++;
    }
    CHECK(on[0][0] == 262 && on[0][1] == 330 && on[1][0] == 330 && on[1][1] == 392);
    CHECK(on[2][0] == 392 && on[2][1] == 524);
    CHECK(offs[0] == 2 && offs[1] == 2 && offs[2] == 2);
    CHECK(start[0][0] == start[1][0] && start[1][0] == start[2][0]);
    CHECK(start[0][1] == start[1][1] && start[1][1] == start[2][1]);
    CHECK(headless_tone_count() - tones <= 1); /* the program's end quiets the speaker, no more */
}

/* Sixteenths at T140 are 1.95 ticks: a voice's notes start at the tick their place in the bar falls in, whatever
   rests and eighths came before. The sound and the silence of each note were rounded apart, so a run of them started
   1, 2 or 3 ticks after the last and two voices' sixteenths up to 2 ticks from each other: a tune in pieces. */
TEST(play_voices_keep_time_with_each_other)
{
    static const char *const lines[] = {
        "10 PLAY \"MB T140 L16 W1 O3 CCCCCCCCCCCCCCCC\", \"T140 L16 O3 C P16 C C P16 C8 C P16 C C C C8 C C\"", nullptr};
    static const int place[2][16] = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                                     {0, 2, 3, 5, 7, 9, 10, 11, 12, 14, 15, -1}};

    qbrt_realtime(false);
    music_clear();
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    headless_run(400);
    qbrt_realtime(true);
    int ons[2] = {0};
    long zero = -1;
    for (size_t i = first; i < headless_voice_count(); i++) {
        const HeadlessVoice h = headless_voice(i);
        if (h.voice > 1 || !h.v.gate || !h.v.hertz || ons[h.voice] >= 16)
            continue;
        zero = zero < 0 ? h.tick : zero;
        CHECK(h.tick - zero == place[h.voice][ons[h.voice]++] * 1950L / 1000);
    }
    CHECK(ons[0] == 16 && ons[1] == 11);
}

/* The synthesizer's clock: its call comes by the count of samples, 291.3 times a second, however the rendering is cut
   up, and a voice it sets sounds from that sample. */
static int clock_calls;
static Synth *clock_synth;
static void clock_tick(void)
{
    if (++clock_calls == 100) {
        const DevVoice v = note(441, DEV_SAW, 0x00F0);
        synth_voice(clock_synth, 0, &v);
    }
}

TEST(synth_clock_ticks_by_its_samples)
{
    Synth s;
    synth_init(&s, RATE);
    synth_clock(&s, clock_tick, 4661);
    clock_calls = 0;
    clock_synth = &s;
    for (int at = 0, piece = 1; at < RATE; at += piece, piece = piece * 7 % 601 + 1)
        synth_render(&s, buf + at, piece < RATE - at ? piece : RATE - at);
    CHECK(clock_calls == 292); /* at sample 0, then every 151.4 */
    int first = 0;
    while (first < RATE && buf[first] == 0)
        first++;
    CHECK(first >= 99 * 1514 / 10 - 4 && first <= 99 * 1514 / 10 + 4);
}

/* A synthesizer's notes count in 16ths of a clock tick: sixteenths at T140 start 31 parts apart (1.95 ticks), where
   the clock tick alone put them 1 or 2 ticks apart. */
void music_fine_tick(void);
TEST(play_synth_notes_start_between_clock_ticks)
{
    static const char *const lines[] = {"10 PLAY \"MB T140 L16 W1 O3 CCCC\"", nullptr};

    qbrt_realtime(false);
    music_clear();
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    int ons[3];
    for (int k = 0; k < 3; k++) {
        for (int i = 0; i < (k == 0 ? 1 : k == 1 ? 30 : 1); i++)
            music_fine_tick();
        ons[k] = 0;
        for (size_t i = first; i < headless_voice_count(); i++)
            ons[k] += headless_voice(i).v.gate && headless_voice(i).v.hertz;
    }
    CHECK(ons[0] == 1 && ons[1] == 1 && ons[2] == 2);
    music_clear();
    headless_run(20); /* the clock's tick takes the queues back */
    qbrt_realtime(true);
}

/* The new commands set the voice: W wave, Q pulse width, V volume, H envelope, Y filter. */
TEST(play_sets_the_voice)
{
    static const char *const lines[] = {"10 PLAY \"MF W2 Q25 V12 H1,2,10,3 Y1,800,9 C P64\"", nullptr};

    qbrt_realtime(false);
    music_clear(); /* an earlier test's music */
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    qbrt_realtime(true);
    CHECK(headless_voice_count() > first);
    const DevVoice v = headless_voice(first).v;
    CHECK(v.wave == DEV_SAW && v.width == 25 * 4096 / 100 && v.volume == 12 && v.envelope == 0x12A3);
    CHECK(v.filter == DEV_LOW_PASS && v.cutoff == 800 && v.resonance == 9 && v.gate == 1);
}

/* W4 is FM, Z its modulator's ratio and depth. */
TEST(play_sets_fm)
{
    static const char *const lines[] = {"10 PLAY \"MF W4 Z3,11 C P64\"", nullptr};
    static const char *const ratio[] = {"10 PLAY \"W4 Z0 C\"", nullptr};
    static const char *const depth[] = {"10 PLAY \"W4 Z1,16 C\"", nullptr};

    qbrt_realtime(false);
    music_clear();
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    qbrt_realtime(true);
    CHECK(headless_voice_count() > first);
    const DevVoice v = headless_voice(first).v;
    CHECK(v.wave == DEV_FM && v.ratio == 3 && v.depth == 11);
    CHECK(unit_run(S, ratio) == E_USR);
    CHECK(unit_run(S, depth) == E_USR);
}

TEST(play_voice_out_of_range)
{
    static const char *const wave[] = {"10 PLAY \"W5 C\"", nullptr};
    static const char *const envelope[] = {"10 PLAY \"H1,2 C\"", nullptr};
    static const char *const seven[] = {"10 PLAY \"C\", \"D\", \"E\", \"F\", \"G\", \"A\", \"B\"", nullptr};
    static const char *const trailing[] = {"10 PLAY \"C\", ", nullptr};

    CHECK(unit_run(S, wave) == E_USR);
    CHECK(unit_run(S, envelope) == E_USR);
    CHECK(unit_run(S, seven) != ST_OK);
    CHECK(unit_run(S, trailing) != ST_OK);
}

/* What a program set of the synthesizer ends with it: the next one's PLAY is the speaker's. */
TEST(play_voice_ends_with_the_program)
{
    static const char *const synth[] = {"10 PLAY \"MF O4 V9 C P64\"", nullptr};
    static const char *const old[] = {"10 PLAY \"MF O4 C P64\"", nullptr};

    qbrt_realtime(false);
    music_clear(); /* an earlier test's music */
    CHECK(unit_run(S, synth) == ST_OK);
    const size_t tones = headless_tone_count(), voices = headless_voice_count();
    CHECK(unit_run(S, old) == ST_OK);
    qbrt_realtime(true);
    CHECK(headless_voice_count() == voices);
    CHECK(headless_tone_count() > tones && headless_tone(tones).hertz == 524);
}

/* Six strings, six voices, starting together. */
TEST(play_of_six_strings_plays_six_voices)
{
    static const char *const lines[] = {
        "10 PLAY \"MF T120 L4 O3 C\", \"T120 L4 O3 E\", \"T120 L4 O3 G\", \"T120 L4 O4 C\", \"T120 L4 O4 E\", "
        "\"T120 L4 O4 G\"",
        nullptr};
    static const unsigned hertz[6] = {262, 330, 392, 524, 660, 784};

    qbrt_realtime(false);
    music_clear(); /* an earlier test's music */
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    headless_run(200);
    qbrt_realtime(true);

    long start[6] = {-1, -1, -1, -1, -1, -1};
    for (size_t i = first; i < headless_voice_count(); i++) {
        const HeadlessVoice h = headless_voice(i);
        if (h.v.gate && start[h.voice] < 0) {
            start[h.voice] = h.tick;
            CHECK(h.v.hertz == hertz[h.voice]);
        }
    }
    for (int v = 0; v < 6; v++)
        CHECK(start[v] >= 0 && start[v] == start[0]);
}

/* PLAY , , a$ plays voice 3 alone: the first voice's music goes on, its notes neither cut nor joined. */
TEST(play_left_out_strings_leave_their_voices)
{
    static const char *const lines[] = {"10 PLAY \"MB T120 L4 O3 C D E\": n = PLAY(0)",
                                        "20 PLAY , , \"T120 L4 O3 G\": PRINT n; PLAY(0)", nullptr};

    qbrt_realtime(false);
    music_clear(); /* an earlier test's music */
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 3  3 \r");
    headless_run(200);
    qbrt_realtime(true);

    int third = 0, others = 0;
    for (size_t i = first; i < headless_voice_count(); i++) {
        const HeadlessVoice h = headless_voice(i);
        third += h.voice == 2 && h.v.gate && h.v.hertz == 392;
        others += h.voice != 2;
    }
    CHECK(third == 1 && others == 0);
}
