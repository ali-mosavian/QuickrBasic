/* The synthesizer (plat/synth) rendered offline, and PLAY's voices on llrm's queues (play.c) through the headless
   device's logs. */
#include <math.h>
#include <stdio.h>
#include <string.h>
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
    DevVoice v[DEV_LAYERS];
    long tick;
} HeadlessVoice;
size_t headless_tone_count(void);
HeadlessTone headless_tone(size_t index);
size_t headless_voice_count(void);
HeadlessVoice headless_voice(size_t index);
void headless_voice_forget(void);
void headless_reset(void);
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
    CHECK(high > (RATE - RATE / 10) * 23 / 100 && high < (RATE - RATE / 10) * 26 / 100); /* an edge's sample is 0 */

    sound(&s, note(441, DEV_TRIANGLE, 0x00F0), RATE);
    int turns = 0;
    for (int i = RATE / 10 + 1; i < RATE - 1; i++)
        turns += (buf[i] - buf[i - 1] > 0) != (buf[i + 1] - buf[i] > 0);
    CHECK(turns >= 2 * 441 * 9 / 10 - 2 && turns <= 2 * 441 * 9 / 10 + 2);

    sound(&s, note(441, DEV_SAW, 0x00F0), RATE);
    int drops = 0;
    for (int i = RATE / 10 + 1; i < RATE; i++)
        drops += buf[i] < buf[i - 1];
    CHECK(drops >= 441 * 9 / 10 - 1 && drops <= 2 * 441 * 9 / 10 + 2); /* its fall rounded: a sample or two */
}

/* FM (W4): with no depth a sine, which turns twice a period and has no step a saw's size; a modulator at three times
   the note bends it into more turns, the period the same; and the bend falls at its own rate, so a note's tail is
   the sine again while it is as loud. */
TEST(synth_fm_bends_a_sine)
{
    Synth s;
    DevVoice v = note(441, DEV_FM, 0x00F0);
    v.ratio = 3;
    v.fall = 0x0F;
    sound(&s, v, RATE);
    int turns = 0, step = 0;
    for (int i = RATE / 10 + 1; i < RATE - 1; i++) {
        turns += (buf[i] - buf[i - 1] > 0) != (buf[i + 1] - buf[i] > 0);
        step = abs(buf[i] - buf[i - 1]) > step ? abs(buf[i] - buf[i - 1]) : step;
    }
    CHECK(peak(buf, RATE) > 11800 && peak(buf, RATE) <= 12000);
    CHECK(turns >= 2 * 441 * 9 / 10 - 2 && turns <= 2 * 441 * 9 / 10 + 2);
    CHECK(step < 12000 / 10);

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

    v.fall = 0x60; /* the modulator falls to nothing in 204 ms */
    sound(&s, v, RATE);
    int early = 0, late = 0;
    for (int i = 1; i < RATE / 50 - 1; i++)
        early += (buf[i] - buf[i - 1] > 0) != (buf[i + 1] - buf[i] > 0);
    for (int i = RATE / 2 + 1; i < RATE / 2 + RATE / 50 - 1; i++)
        late += (buf[i] - buf[i - 1] > 0) != (buf[i + 1] - buf[i] > 0);
    CHECK(early > late);
}

/* A voice of layers sounds as its layers would as voices of their own, from their first note, added. */
TEST(synth_layers_sound_as_voices_added)
{
    static int16_t apart[RATE / 2];
    Synth s;
    DevVoice v[3] = {note(200, DEV_FM, 0x0A50), note(200, DEV_NOISE, 0x1630), note(200, DEV_SAW, 0x0400)};
    v[0].depth = 3;
    v[0].volume = 6;
    v[0].layers = 2;
    v[1].pitch = 1500;
    v[1].volume = 5;
    v[1].filter = DEV_LOW_PASS;
    v[1].cutoff = 3000;
    v[2].pitch = 276;
    v[2].volume = 3;
    synth_init(&s, RATE);
    for (unsigned k = 0; k < 3; k++) {
        DevVoice one = v[k];
        one.layers = 0;
        synth_voice(&s, k, &one);
    }
    synth_render(&s, apart, RATE / 2);
    synth_init(&s, RATE);
    s.osc[0].phase = 12345; /* a voice's last note left it anywhere */
    synth_voice(&s, 0, v);
    synth_render(&s, buf, RATE / 2);
    CHECK(peak(buf, RATE / 2) > 3000 && !memcmp(buf, apart, sizeof apart));
}

/* A voice's layers start together at each note, so its hits sound alike. Two voices as a hit's thump and hiss started
   wherever their last notes left them: one pair's first 60 ms came 3.4 to 6.1 dB from the recording by phase alone. */
TEST(synth_layered_hits_sound_alike)
{
    static int16_t first[RATE / 10];
    Synth s;
    DevVoice v[2] = {note(147, DEV_FM, 0x0300), note(147, DEV_NOISE, 0x0300)};
    v[0].depth = 0;
    v[0].layers = 1;
    v[1].pitch = 3000;
    v[1].filter = DEV_BAND_PASS;
    v[1].cutoff = 2400;
    v[1].resonance = 6;
    synth_init(&s, RATE);
    for (int hit = 0; hit < 3; hit++) {
        v[0].gate = 1;
        v[0].trigger = (unsigned char)(hit + 1);
        synth_voice(&s, 0, v);
        synth_render(&s, hit ? buf : first, RATE / 10);
        CHECK(peak(first, RATE / 10) > 1000);
        if (hit)
            CHECK(!memcmp(buf, first, sizeof first));
        v[0].gate = 0;
        synth_voice(&s, 0, v);
        synth_render(&s, buf, RATE / 3 + 777 * hit); /* the hits apart by no whole period */
    }
}

/* Z's sixth number: where the bend starts, under its level, so it rises to it: a tone that brightens, as a snare's
   hiss swells in. The bend only fell. */
TEST(synth_fm_bend_rises)
{
    Synth s;
    DevVoice v = note(441, DEV_FM, 0x00F0);
    v.depth = 10;
    v.fall = 0x6F; /* to full in 204 ms */
    v.dip = 15;    /* from a plain sine */
    sound(&s, v, RATE);
    int early = 0, late = 0;
    for (int i = 1; i < RATE / 50 - 1; i++)
        early += (buf[i] - buf[i - 1] > 0) != (buf[i + 1] - buf[i] > 0);
    for (int i = RATE / 2 + 1; i < RATE / 2 + RATE / 50 - 1; i++)
        late += (buf[i] - buf[i - 1] > 0) != (buf[i + 1] - buf[i] > 0);
    CHECK(early <= 2 * 441 / 50 + 2 && late > early * 2);
}

/* How far under a held note's whole sound the part of it off its harmonics is, in dB (0 is all of it): a second of
   `v` from its second second, each harmonic under half the rate measured. 1000 Hz is no divisor of the rate, so what
   folds back from over half of it lands between the harmonics. */
static double off_the_note(const DevVoice v)
{
    static int16_t two[2 * RATE];
    Synth s;
    synth_init(&s, RATE);
    synth_voice(&s, 0, &v);
    synth_render(&s, two, 2 * RATE);
    const int16_t *const x = two + RATE;
    double all = 0, on = 0, mean = 0;
    for (int i = 0; i < RATE; i++)
        mean += x[i];
    mean /= RATE;
    for (int i = 0; i < RATE; i++)
        all += (x[i] - mean) * (x[i] - mean);
    for (unsigned f = v.hertz; f < RATE / 2; f += v.hertz) {
        double re = 0, im = 0;
        for (int i = 0; i < RATE; i++) {
            re += x[i] * cos(2 * M_PI * f * i / RATE);
            im += x[i] * sin(2 * M_PI * f * i / RATE);
        }
        on += 2 * (re * re + im * im) / RATE;
    }
    return 10 * log10((all - on) / all + 1e-12);
}

/* Z's ratio with hundredths: the modulator off the note's harmonics, as a drum's or a bell's modes are. The ratio was
   whole, so every FM tone was a harmonic series: the snare's lines at 199, 234 and 319 Hz were no patch's. */
TEST(synth_fm_fine_ratio_leaves_the_harmonics)
{
    DevVoice v = note(200, DEV_FM, 0x00F0);
    v.ratio = 1;
    v.depth = 6;
    v.fall = 0x0F;
    CHECK(off_the_note(v) < -40);
    v.fine = 41;
    CHECK(off_the_note(v) > -3);
    v.ratio = 0;
    v.fine = 50; /* half the note: its odd lines between the harmonics, -3.7 dB of a depth of 3 radians */
    CHECK(off_the_note(v) > -6);
}

/* What a voice sounds that is not its note. A pulse and a saw jumped between two samples, and a sixth of a 1 kHz
   note's power (-16 dB) was tones folded back from over half the rate; the sine's table of 1024 left -55 dB; and
   feedback 5 bent the modulator twice as far as meant, into noise, -7 dB: a brass that hissed. */
TEST(synth_voices_sound_their_note)
{
    DevVoice v = note(1000, DEV_PULSE, 0x00F0);
    v.width = 4096 * 30 / 100;
    CHECK(off_the_note(v) < -30);
    v.wave = DEV_SAW;
    CHECK(off_the_note(v) < -30);
    v.wave = DEV_FM;
    v.ratio = 1;
    v.fall = 0x0F;
    CHECK(off_the_note(v) < -62);
    v.hertz = 500;
    v.depth = 9;
    v.fall = 0x3B;
    v.feedback = 5;
    CHECK(off_the_note(v) < -40);
}

/* The spacing of a second's rising zero crossings, the least and the most, from sample `from`. */
static void spacing(const int from, int *const least, int *const most)
{
    int last = -1;

    *least = RATE;
    *most = 0;
    for (int i = from + 1; i < RATE; i++)
        if (buf[i - 1] < 0 && buf[i] >= 0) {
            if (last >= 0) {
                *least = i - last < *least ? i - last : *least;
                *most = i - last > *most ? i - last : *most;
            }
            last = i;
        }
}

/* A modulator that bends itself is brighter: the carrier's steepest step grows. */
TEST(synth_fm_feedback_brightens)
{
    Synth s;
    DevVoice v = note(441, DEV_FM, 0x00F0);
    int steep[2];

    v.ratio = 1;
    v.depth = 4;
    v.fall = 0x0F;
    for (int f = 0; f < 2; f++) {
        v.feedback = (unsigned char)(f * 6);
        sound(&s, v, RATE);
        steep[f] = 0;
        for (int i = RATE / 10; i < RATE; i++)
            steep[f] = abs(buf[i] - buf[i - 1]) > steep[f] ? abs(buf[i] - buf[i - 1]) : steep[f];
    }
    CHECK(steep[1] > steep[0] * 13 / 10);
}

/* Vibrato: a 441 Hz period is 100 samples every time; at depth 15 it runs from 97 to 103, but not in the note's first
   fifth of a second. */
TEST(synth_vibrato_wavers_after_a_wait)
{
    Synth s;
    DevVoice v = note(441, DEV_TRIANGLE, 0x00F0);
    int least, most;

    sound(&s, v, RATE);
    spacing(RATE / 4, &least, &most);
    CHECK(most - least <= 1);
    v.vibrato = 15;
    sound(&s, v, RATE);
    spacing(RATE / 4, &least, &most);
    CHECK(least <= 98 && most >= 102 && least >= 96 && most <= 104);
    synth_init(&s, RATE);
    synth_voice(&s, 0, &v);
    synth_render(&s, buf, RATE / 6);
    int last = -1, off = 0;
    for (int i = 1; i < RATE / 6; i++)
        if (buf[i - 1] < 0 && buf[i] >= 0) {
            off += last >= 0 && abs(i - last - 100) > 1;
            last = i;
        }
    CHECK(off == 0);
}

/* How bright a stretch is: the power of its steps against its own. */
static double brightness(const int16_t *const x, const int n)
{
    double steps = 0, power = 0;
    for (int i = 1; i < n; i++) {
        steps += (double)(x[i] - x[i - 1]) * (x[i] - x[i - 1]);
        power += (double)x[i] * x[i];
    }
    return steps / (power + 1);
}

/* The loudest sample of each 10 ms of the second from `from`. */
static void tens(const int from, int *const top)
{
    for (int k = 0; k < 100; k++)
        top[k] = peak(buf + from + k * RATE / 100, RATE / 100);
}

/* U's second, third and fourth numbers: the wobble's rate, and the loudness and the cutoff wavering with it. U was the
   one wobble, the pitch's, at 6 Hz. */
TEST(synth_tremolo_and_wobble_at_their_rate)
{
    static int16_t two[2 * RATE];
    Synth s;
    DevVoice v = note(441, DEV_TRIANGLE, 0x00F0);
    int top[100], least = 1 << 30, most = 0, turns = 0;
    v.rate = 100; /* 10 Hz */
    v.tremolo = 8;
    sound(&s, v, RATE / 2 + RATE);
    tens(RATE / 2, top);
    for (int k = 0; k < 100; k++) {
        least = top[k] < least ? top[k] : least;
        most = top[k] > most ? top[k] : most;
        turns += k > 0 && k < 99 && top[k] > top[k - 1] && top[k] >= top[k + 1];
    }
    CHECK(most > least * 5 / 2 && most < least * 7 / 2 && turns >= 9 && turns <= 11); /* 1.5 against 0.5 */

    v = note(110, DEV_SAW, 0x00F0);
    v.filter = DEV_LOW_PASS;
    v.cutoff = 800;
    v.rate = 20; /* 2 Hz */
    v.wobble = 12;
    synth_init(&s, RATE);
    synth_voice(&s, 0, &v);
    synth_render(&s, two, 2 * RATE);
    double dull = 1e30, bright = 0;
    for (int k = 0; k < 50; k++) {
        const double b = brightness(two + RATE / 2 + k * RATE / 50, RATE / 50);
        dull = b < dull ? b : dull;
        bright = b > bright ? b : bright;
    }
    CHECK(bright > dull * 8);
}

/* A unison: two saws 16 cents apart beat four times a second, so the sound swells and thins; one alone is as loud
   all through. */
TEST(synth_unison_beats)
{
    Synth s;
    DevVoice v = note(441, DEV_SAW, 0x00F0);
    int least[2], most[2];

    for (int u = 0; u < 2; u++) {
        v.spread = (unsigned char)(u * 8);
        sound(&s, v, RATE);
        least[u] = 32767;
        most[u] = 0;
        for (int at = RATE / 10; at + RATE / 50 <= RATE; at += RATE / 50) {
            const int top = peak(buf + at, RATE / 50);
            least[u] = top < least[u] ? top : least[u];
            most[u] = top > most[u] ? top : most[u];
        }
    }
    CHECK(least[0] > most[0] * 9 / 10);
    CHECK(least[1] < most[1] * 6 / 10);
}

/* A glide: the second note climbs from the first's pitch over 150 ms, so its first 50 ms hold fewer periods than
   its last; and a note told to fall from twice its pitch starts with more. */
TEST(synth_glide_slides_to_the_note)
{
    Synth s;
    DevVoice v = note(220, DEV_TRIANGLE, 0x00F0);

    v.glide = 9;
    synth_init(&s, RATE);
    synth_voice(&s, 0, &v);
    synth_render(&s, buf, RATE / 2);
    CHECK(rises(buf + RATE / 4, RATE / 20) >= 10 && rises(buf + RATE / 4, RATE / 20) <= 12); /* no last note: no slide */
    v.hertz = 440;
    v.trigger = 2;
    synth_voice(&s, 0, &v);
    synth_render(&s, buf, RATE / 2);
    const int early = rises(buf, RATE / 20), late = rises(buf + RATE / 4, RATE / 20);
    CHECK(early >= 11 && early <= 15 && late >= 21 && late <= 23);

    v.from = 4;
    v.trigger = 3;
    synth_voice(&s, 0, &v);
    synth_render(&s, buf, RATE / 2);
    CHECK(rises(buf, RATE / 20) >= 34 && rises(buf + RATE / 4, RATE / 20) <= 23);

    /* from an octave under its own pitch: a scoop up into the note, which a slide from over could not make */
    v.under = 12;
    v.trigger = 4;
    synth_voice(&s, 0, &v);
    synth_render(&s, buf, RATE / 2);
    const int scoop = rises(buf, RATE / 20);
    CHECK(scoop >= 11 && scoop <= 16 && rises(buf + RATE / 4, RATE / 20) >= 21);
}

/* K's fourth number: the slide starts at so many hundredths of the pitch. From over it started at 1.25 times at the
   least: the tom's fall of 1.4 times was no patch's. */
TEST(synth_glide_from_hundredths)
{
    Synth s;
    DevVoice v = note(1000, DEV_TRIANGLE, 0x00F0);
    v.glide = 4; /* 40 ms */
    v.over = 140;
    sound(&s, v, RATE / 5);
    const int early = rises(buf, RATE / 200), late = rises(buf + RATE / 10, RATE / 100);
    CHECK(early >= 6 && early <= 7 && late >= 9 && late <= 10);
    v.over = 71;
    sound(&s, v, RATE / 5);
    CHECK(rises(buf, RATE / 200) <= 4 && rises(buf + RATE / 10, RATE / 100) >= 9);
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
    CHECK(full > 5600 && half_way > full * 4 / 10 && half_way < full * 6 / 10);
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

/* H's fifth number: from the sustain the level falls on while the note is held, at a second slope. One decay to a
   sustain of 16 even steps rang 23.5 dB under the peak at the quietest: the snare falls 20 dB in 100 ms, then rings
   35 to 47 dB under till 400 ms. */
TEST(synth_envelope_falls_on_from_its_sustain)
{
    Synth s;
    DevVoice v = note(1000, DEV_SAW, 0x0280); /* 48 ms to a sustain of 8 */
    sound(&s, v, RATE / 2);
    const int held = peak(buf + RATE / 2 - RATE / 100, RATE / 100);
    v.fade = 1 + 9; /* 750 ms from full to nothing */
    sound(&s, v, RATE / 2);
    const int at = peak(buf + RATE / 10 - RATE / 100, RATE / 100), later = peak(buf + RATE / 2 - RATE / 100, RATE / 100);
    /* SID's curve: under 93 of 255 the fall halves, under 54 it quarters: a third of the sustain at 450 ms */
    CHECK(at < held && at > held * 3 / 4 && later > held * 3 / 10 && later < held * 4 / 10);
}

/* V's hundredths: a level between two steps. V was 0 to 15 in even steps: V4 to V5 is 1.9 dB, and a hat that
   belonged between them could not be trimmed. */
TEST(synth_volume_in_hundredths)
{
    Synth s;
    int tops[3];
    for (int k = 0; k < 3; k++) {
        DevVoice v = note(440, DEV_PULSE, 0x00F0);
        v.volume = (unsigned char)(k == 2 ? 5 : 4);
        v.loud = (unsigned char)(k == 1 ? 50 : 0);
        sound(&s, v, RATE / 10);
        tops[k] = peak(buf + RATE / 20, RATE / 20);
    }
    CHECK(tops[0] < tops[2] && abs(tops[1] * 2 - tops[0] - tops[2]) <= 4);
}

/* Y's fourth and fifth numbers: the cutoff a note starts at, and how long it takes to the cutoff. A saw swept down
   from 6000 to 400 Hz starts bright and ends as one filtered at 400; swept up, the other way. The cutoff was fixed for
   the note: a pluck's brightness fell with its loudness or not at all. */
TEST(synth_cutoff_sweeps)
{
    Synth s;
    DevVoice v = note(110, DEV_SAW, 0x00F0);
    v.filter = DEV_LOW_PASS;
    v.cutoff = 400;
    sound(&s, v, RATE);
    const double plain = brightness(buf + RATE / 2, RATE / 50);
    v.sweep = 6000;
    v.swept = 7; /* 240 ms */
    sound(&s, v, RATE);
    const double start = brightness(buf + RATE / 100, RATE / 50), end = brightness(buf + RATE / 2, RATE / 50);
    CHECK(start > end * 4 && fabs(end - plain) < plain / 20);
    v.cutoff = 6000;
    v.sweep = 400;
    sound(&s, v, RATE);
    CHECK(brightness(buf + RATE / 100, RATE / 50) * 4 < brightness(buf + RATE / 2, RATE / 50));
}

/* 32 voices at full volume, their sum six times what a sample holds: they were clipped flat, nine samples in ten at
   the rail. The limiter turns the mix down instead, to just under full, and one voice after them is back at twice
   its sum within a second. */
TEST(synth_limits_many_voices)
{
    static Synth s;
    static int16_t out[RATE];
    int flat = 0;

    synth_init(&s, RATE);
    for (unsigned k = 0; k < DEV_VOICES; k++) {
        const DevVoice v = note(110 + 7 * k, DEV_PULSE, 0x00F0);

        synth_voice(&s, k, &v);
    }
    synth_render(&s, out, RATE);
    for (int i = RATE / 10; i < RATE; i++)
        flat += abs(out[i]) >= 32767;
    CHECK(flat < RATE / 1000);
    CHECK(peak(out + RATE / 10, RATE - RATE / 10) > 30000);

    for (unsigned k = 1; k < DEV_VOICES; k++) {
        const DevVoice off = {0};

        synth_voice(&s, k, &off);
    }
    synth_render(&s, out, RATE);
    CHECK(peak(out + RATE - RATE / 10, RATE / 10) == 12000);
}

/* A voice the room answers (R) sounds on after it is cut, fading, and is gone in three seconds; one it does not
   answer stops dead, as every voice did: between its notes a tune was silence, where a recording has a room. */
TEST(synth_room_answers_a_voice)
{
    static Synth s;
    static int16_t out[RATE];
    int tail[2], late = 0, end = 0;

    for (int wet = 0; wet < 2; wet++) {
        DevVoice v = note(441, DEV_SAW, 0x00F0);
        const DevVoice off = {0};

        v.reverb = wet ? 8 : 0;
        synth_init(&s, RATE);
        synth_voice(&s, 0, &v);
        synth_render(&s, out, RATE / 2);
        synth_voice(&s, 0, &off);
        synth_render(&s, out, RATE);
        tail[wet] = peak(out + RATE / 10, RATE / 10);
        late = peak(out + RATE - RATE / 10, RATE / 10);
        synth_render(&s, out, RATE);
        synth_render(&s, out, RATE);
        synth_render(&s, out, RATE);
        end = peak(out, RATE);
    }
    CHECK(tail[0] == 0);
    CHECK(tail[1] > 300);
    CHECK(late > 0 && late < tail[1] / 4);
    CHECK(end == 0);
}

/* In stereo a voice is where its pan puts it: all of it on its side, less on the other by how far it is, none at the
   end; in the middle, and in mono whatever its pan, it is the same on both. */
TEST(synth_stereo_puts_a_voice_where_its_pan_is)
{
    static Synth s;
    static int16_t both[RATE / 5], left[RATE / 10], right[RATE / 10], mono[RATE / 10];
    static const unsigned char pans[] = {1, 12, 15, 8, 0};
    int tops[5][2];

    for (int k = 0; k < 5; k++) {
        DevVoice v = note(441, DEV_SAW, 0x00F0);

        v.pan = pans[k];
        synth_init(&s, RATE);
        synth_voice(&s, 0, &v);
        synth_render_stereo(&s, both, RATE / 10);
        for (int i = 0; i < RATE / 10; i++) {
            left[i] = both[i * 2];
            right[i] = both[i * 2 + 1];
        }
        tops[k][0] = peak(left, RATE / 10);
        tops[k][1] = peak(right, RATE / 10);
        synth_init(&s, RATE);
        synth_voice(&s, 0, &v);
        synth_render(&s, mono, RATE / 10);
        CHECK(peak(mono, RATE / 10) == (tops[k][0] > tops[k][1] ? tops[k][0] : tops[k][1]));
    }
    CHECK(tops[0][0] > 1000 && tops[0][1] == 0);
    CHECK(tops[1][1] == tops[0][0] && tops[1][0] > tops[1][1] * 2 / 5 && tops[1][0] < tops[1][1] / 2);
    CHECK(tops[2][1] == tops[0][0] && tops[2][0] == 0);
    CHECK(tops[3][0] == tops[3][1] && tops[3][0] == tops[0][0]);
    CHECK(tops[4][0] == tops[4][1] && tops[4][0] == tops[0][0]);
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
    CHECK(hash == 332032406u);
}

/* Every wave under every filter, six voices at once with the speaker, released halfway, one cut at three quarters,
   rendered in uneven pieces: the samples as they were before rendering went a voice a block at a time. */
TEST(synth_six_voices_render_as_before)
{
    Synth s;
    synth_init(&s, 15873);
    synth_tone(&s, 440);
    for (unsigned k = 0; k < 6; k++) {
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
    CHECK(hash == 2292971059u); /* since the mix is doubled and limited */
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
        for (int i = 0; i < TICK_SAMPLES; i++) /* the mix is twice its sum */
            CHECK(now[i] == old[i] * 2);
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
        if (h.v[0].gate && ons[h.voice] < 2) {
            start[h.voice][ons[h.voice]] = h.tick;
            on[h.voice][ons[h.voice]++] = h.v[0].hertz;
        } else if (!h.v[0].gate && h.v[0].hertz)
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
        if (h.voice > 1 || !h.v[0].gate || !h.v[0].hertz || ons[h.voice] >= 16)
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
            ons[k] += headless_voice(i).v[0].gate && headless_voice(i).v[0].hertz;
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
    const DevVoice v = headless_voice(first).v[0];
    CHECK(v.wave == DEV_SAW && v.width == 25 * 4096 / 100 && v.volume == 12 && v.envelope == 0x12A3);
    CHECK(v.filter == DEV_LOW_PASS && v.cutoff == 800 && v.resonance == 9 && v.gate == 1);
}

/* W4 is FM, Z its modulator: ratio, depth, decay, level and feedback; U is vibrato, K a glide, I a unison. */
TEST(play_sets_fm)
{
    static const char *const lines[] = {"10 PLAY \"MF W4 Z3,11,6,2,5 U7 K4,8,5 I9 R6 S3 C P64\"", nullptr};
    static const char *const aside[] = {"10 PLAY \"S0 C\"", nullptr};
    static const char *const fed[] = {"10 PLAY \"W4 Z1,2,3,4,8 C\"", nullptr};
    static const char *const waver[] = {"10 PLAY \"U16 C\"", nullptr};
    static const char *const ratio[] = {"10 PLAY \"W4 Z0 C\"", nullptr};
    static const char *const depth[] = {"10 PLAY \"W4 Z1,16 C\"", nullptr};

    qbrt_realtime(false);
    music_clear();
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    qbrt_realtime(true);
    CHECK(headless_voice_count() > first);
    const DevVoice v = headless_voice(first).v[0];
    CHECK(v.wave == DEV_FM && v.ratio == 3 && v.depth == 11);
    CHECK(v.fall == 0x62 && v.feedback == 5 && v.vibrato == 7 && v.glide == 4 && v.from == 8 && v.spread == 9);
    CHECK(v.reverb == 6 && v.under == 5 && v.pan == 3);
    CHECK(unit_run(S, aside) == E_USR);
    CHECK(unit_run(S, fed) == E_USR);
    CHECK(unit_run(S, waver) == E_USR);
    CHECK(unit_run(S, ratio) == E_USR);
    CHECK(unit_run(S, depth) == E_USR);
}

/* Z's ratio takes hundredths after a point: Z1.41, Z0.5. */
TEST(play_sets_fm_fine)
{
    static const char *const lines[] = {"10 PLAY \"MF W4 Z1.41,6 C Z0.5 C Z2.05 C P64\"", nullptr};
    static const char *const wrong[][2] = {{"10 PLAY \"W4 Z1.415 C\"", nullptr}, {"10 PLAY \"W4 Z1. C\"", nullptr},
                                           {"10 PLAY \"W4 Z0.0 C\"", nullptr}, {"10 PLAY \"W4 Z16.5 C\"", nullptr}};

    qbrt_realtime(false);
    music_clear();
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    qbrt_realtime(true);
    unsigned got[3][3], n = 0;
    for (size_t i = first; i < headless_voice_count() && n < 3; i++) {
        const DevVoice v = headless_voice(i).v[0];
        if (v.gate && v.hertz) {
            got[n][0] = v.ratio;
            got[n][1] = v.fine;
            got[n++][2] = v.depth;
        }
    }
    CHECK(n == 3 && got[0][0] == 1 && got[0][1] == 41 && got[0][2] == 6);
    CHECK(got[1][0] == 0 && got[1][1] == 50 && got[2][0] == 2 && got[2][1] == 5);
    for (int i = 0; i < 4; i++)
        CHECK(unit_run(S, wrong[i]) == E_USR);
}

/* J chooses the layer the commands after it set, to the string's end; its pitch sounds the layer, 0 silences it and
   those after it. U, I, R and S are the voice's. */
TEST(play_sets_layers)
{
    static const char *const lines[] = {
        "10 PLAY \"MF W4 Z1,0 H0,4,0,3 J1,150 W3 H0,2,0,2 V5 S3 J2,250 W1 C J1,0 C J0,200 C P64\": PLAY \"V12 C P64\"",
        nullptr};
    static const char *const wrong[][2] = {{"10 PLAY \"J4 C\"", nullptr}, {"10 PLAY \"J2,100 C\"", nullptr},
                                           {"10 PLAY \"J1,1601 C\"", nullptr}};

    qbrt_realtime(false);
    music_clear();
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    qbrt_realtime(true);
    HeadlessVoice on[4];
    unsigned n = 0;
    for (size_t i = first; i < headless_voice_count() && n < 4; i++)
        if (headless_voice(i).v[0].gate && headless_voice(i).v[0].hertz)
            on[n++] = headless_voice(i);
    CHECK(n == 4);
    const DevVoice *const v = on[0].v;
    CHECK(v[0].layers == 2 && v[0].wave == DEV_FM && v[0].pan == 3 && v[0].envelope == 0x0403 && !v[0].pitch);
    CHECK(v[1].wave == DEV_NOISE && v[1].pitch == 150 && v[1].volume == 5 && v[1].envelope == 0x0202);
    CHECK(v[2].wave == DEV_TRIANGLE && v[2].pitch == 250);
    CHECK(on[1].v[0].layers == 0 && on[2].v[0].pitch == 200);
    CHECK(on[3].v[0].volume == 12 && on[3].v[0].wave == DEV_FM);
    for (int i = 0; i < 3; i++)
        CHECK(unit_run(S, wrong[i]) == E_USR);
}

/* J1 with no pitch set a layer that never sounded: a tom's noise layer was silent. */
TEST(play_sounds_a_layer_named_alone)
{
    static const char *const lines[] = {"10 PLAY \"MF W4 Z1,0 J1 W3 V14 J0 C P64\"", nullptr};
    static const char *const wrong[][2] = {{"10 PLAY \"J2 C\"", nullptr}};

    qbrt_realtime(false);
    music_clear();
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    qbrt_realtime(true);
    HeadlessVoice on = {};
    for (size_t i = first; i < headless_voice_count() && !on.v[0].gate; i++)
        if (headless_voice(i).v[0].gate && headless_voice(i).v[0].hertz)
            on = headless_voice(i);
    const DevVoice *const v = on.v;
    CHECK(v[0].layers == 1 && v[1].wave == DEV_NOISE && v[1].volume == 14 && !v[1].pitch);
    CHECK(unit_run(S, wrong[0]) == E_USR);
}

/* Z's sixth number, the bend's start; Y's fourth and fifth, the cutoff's start and its time; K's fourth, the slide's
   start in hundredths. */
TEST(play_sets_changes_within_a_note)
{
    static const char *const lines[] = {
        "10 PLAY \"MF W4 Z1,8,3,15,2,4 Y1,800,6,6000,4 K4,0,0,140 C Y1,800,6,0 C P64\"", nullptr};
    static const char *const wrong[][2] = {{"10 PLAY \"Y1,800,6,20 C\"", nullptr},
                                           {"10 PLAY \"W4 Z1,8,3,15,2,16 C\"", nullptr},
                                           {"10 PLAY \"K4,0,0,1601 C\"", nullptr}, {"10 PLAY \"Y1,800,6,900,16 C\"", nullptr}};

    qbrt_realtime(false);
    music_clear();
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    qbrt_realtime(true);
    DevVoice on[2];
    unsigned n = 0;
    for (size_t i = first; i < headless_voice_count() && n < 2; i++)
        if (headless_voice(i).v[0].gate && headless_voice(i).v[0].hertz)
            on[n++] = headless_voice(i).v[0];
    CHECK(n == 2 && on[0].dip == 11 && on[0].feedback == 2 && on[0].sweep == 6000 && on[0].swept == 4);
    CHECK(on[0].over == 140 && on[0].glide == 4 && on[1].sweep == 0 && on[1].swept == 4);
    for (int i = 0; i < 4; i++)
        CHECK(unit_run(S, wrong[i]) == E_USR);
}

/* H's fifth number is the fall on from the sustain; four alone hold it. */
TEST(play_sets_a_second_decay)
{
    static const char *const lines[] = {"10 PLAY \"MF H0,4,8,3,9 C H0,4,8,3 C P64\"", nullptr};
    static const char *const wrong[] = {"10 PLAY \"H0,4,8,3,16 C\"", nullptr};

    qbrt_realtime(false);
    music_clear();
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    qbrt_realtime(true);
    DevVoice on[2];
    unsigned n = 0;
    for (size_t i = first; i < headless_voice_count() && n < 2; i++)
        if (headless_voice(i).v[0].gate && headless_voice(i).v[0].hertz)
            on[n++] = headless_voice(i).v[0];
    CHECK(n == 2 && on[0].envelope == 0x0483 && on[0].fade == 10 && on[1].fade == 0);
    CHECK(unit_run(S, wrong) == E_USR);
}

/* V takes hundredths after a point, under 15. */
TEST(play_sets_volume_in_hundredths)
{
    static const char *const lines[] = {"10 PLAY \"MF V4.5 C V15 C P64\"", nullptr};
    static const char *const wrong[][2] = {{"10 PLAY \"V15.5 C\"", nullptr}, {"10 PLAY \"V4.125 C\"", nullptr}};

    qbrt_realtime(false);
    music_clear();
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    qbrt_realtime(true);
    DevVoice on[2];
    unsigned n = 0;
    for (size_t i = first; i < headless_voice_count() && n < 2; i++)
        if (headless_voice(i).v[0].gate && headless_voice(i).v[0].hertz)
            on[n++] = headless_voice(i).v[0];
    CHECK(n == 2 && on[0].volume == 4 && on[0].loud == 50 && on[1].volume == 15 && on[1].loud == 0);
    for (int i = 0; i < 2; i++)
        CHECK(unit_run(S, wrong[i]) == E_USR);
}

/* U's rate, tremolo and wobble; a number left off keeps its last value. */
TEST(play_sets_movement)
{
    static const char *const lines[] = {"10 PLAY \"MF U5,25,6,4 C U3 C P64\"", nullptr};
    static const char *const wrong[][2] = {{"10 PLAY \"U5,0 C\"", nullptr}, {"10 PLAY \"U5,201 C\"", nullptr},
                                           {"10 PLAY \"U5,60,16 C\"", nullptr}, {"10 PLAY \"U5,60,6,16 C\"", nullptr}};

    qbrt_realtime(false);
    music_clear();
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    qbrt_realtime(true);
    DevVoice on[2];
    unsigned n = 0;
    for (size_t i = first; i < headless_voice_count() && n < 2; i++)
        if (headless_voice(i).v[0].gate && headless_voice(i).v[0].hertz)
            on[n++] = headless_voice(i).v[0];
    CHECK(n == 2 && on[0].vibrato == 5 && on[0].rate == 25 && on[0].tremolo == 6 && on[0].wobble == 4);
    CHECK(on[1].vibrato == 3 && on[1].rate == 25 && on[1].tremolo == 6);
    for (int i = 0; i < 4; i++)
        CHECK(unit_run(S, wrong[i]) == E_USR);
}

TEST(play_voice_out_of_range)
{
    static const char *const wave[] = {"10 PLAY \"W5 C\"", nullptr};
    static const char *const envelope[] = {"10 PLAY \"H1,2 C\"", nullptr};
    static char many[400] = "10 PLAY \"C\"";
    static const char *const seven[] = {many, nullptr};
    static const char *const trailing[] = {"10 PLAY \"C\", ", nullptr};

    for (int v = 0; v < DEV_VOICES; v++) /* a string more than there are voices */
        strcat(many, ", \"C\"");
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

/* A string a voice, all 32 starting together: there were six, then ten. */
TEST(play_of_32_strings_plays_32_voices)
{
    static char line[32 * 24 + 16] = "10 PLAY \"MF T120 L4 N1\"";
    static const char *const lines[] = {line, nullptr};

    for (int v = 1; v < 32; v++)
        snprintf(line + strlen(line), 24, ", \"T120 L4 N%d\"", v + 1);
    qbrt_realtime(false);
    music_clear(); /* an earlier test's music */
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    headless_run(200);
    qbrt_realtime(true);

    long start[32];
    unsigned hertz[32];
    for (int v = 0; v < 32; v++)
        start[v] = -1;
    for (size_t i = first; i < headless_voice_count(); i++) {
        const HeadlessVoice h = headless_voice(i);
        if (h.v[0].gate && start[h.voice] < 0) {
            start[h.voice] = h.tick;
            hertz[h.voice] = h.v[0].hertz;
        }
    }
    for (int v = 0; v < 32; v++) /* each its own note, a semitone over the last */
        CHECK(start[v] >= 0 && start[v] == start[0] && (v == 0 || hertz[v] > hertz[v - 1]));
}

/* PLAY , , a$ plays voice 3 alone: the first voice's music goes on, its notes neither cut nor joined. */
TEST(play_left_out_strings_leave_their_voices)
{
    static const char *const lines[] = {"10 PLAY \"MB T120 L4 O3 C D E\": n = PLAY(0)",
                                        "20 PLAY , , \"T120 L4 O3 G\": PRINT n; PLAY(0)", nullptr};

    qbrt_realtime(false);
    music_clear(); /* an earlier test's music */
    headless_reset(); /* and the part of a tick it left, which the second PLAY(0)'s hundredth could finish */
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 3  3 \r");
    headless_run(200);
    qbrt_realtime(true);

    int third = 0, others = 0;
    for (size_t i = first; i < headless_voice_count(); i++) {
        const HeadlessVoice h = headless_voice(i);
        third += h.voice == 2 && h.v[0].gate && h.v[0].hertz == 392;
        others += h.voice != 2;
    }
    CHECK(third == 1 && others == 0);
}

/* A song's voices are all logged: at 4096 the log dropped the rest unsaid, and a 76-bar song's render stopped at bar
 * 39, the bars after it never measured. */
TEST(headless_logs_a_long_songs_voices)
{
    void dev_voice(unsigned voice, const DevVoice *v);
    const DevVoice v = {.hertz = 440, .gate = 1};
    const size_t first = headless_voice_count();

    for (int i = 0; i < 5000; i++)
        dev_voice(0, &v);
    CHECK(headless_voice_count() == first + 5000);
}

/* A legato note run into its own pitch is held, not struck again: each was a new attack, so a length that is no one
 * note's (nine sixteenths) could not be written, and a held chord was let go a sixteenth early. */
TEST(play_legato_ties_a_pitch_to_itself)
{
    static const char *const lines[] = {"10 PLAY \"W0 ML L4 C C D\"", nullptr};

    qbrt_realtime(false);
    music_clear();
    const size_t first = headless_voice_count();
    CHECK(unit_run(S, lines) == ST_OK);
    headless_run(400);
    qbrt_realtime(true);
    int struck = 0, last = -1;
    unsigned hertz = 0;
    for (size_t i = first; i < headless_voice_count(); i++) {
        const DevVoice v = headless_voice(i).v[0];

        if (v.gate && v.trigger != last) {
            struck++;
            last = v.trigger;
            hertz = v.hertz;
        }
    }
    CHECK(struck == 2 && hertz == 294);
}

/* PLAY's voices as the Mac plays them: the synthesizer's clock ticks the music, and each voice a tick sets goes to it
   at once. */
static Synth *played;
static size_t played_from;
static void play_tick(void)
{
    music_fine_tick();
    for (; played_from < headless_voice_count(); played_from++) {
        const HeadlessVoice h = headless_voice(played_from);
        synth_voice(played, h.voice, h.v);
    }
}

/* PLAY's `strings` rendered for 2.5 s at `rate`, `sides` 2 stereo: the samples' hash. */
static uint32_t played_hash(Session *const S, const char *const strings, const uint32_t rate, const int sides)
{
    static Synth s;
    static int16_t out[1000 * 2];
    char line[600];
    const char *const lines[] = {line, nullptr};

    snprintf(line, sizeof line, "10 PLAY \"MB\": PLAY \"%s\"", strings);
    music_clear();
    headless_voice_forget();
    played_from = 0;
    CHECK(unit_run(S, lines) == ST_OK);
    synth_init(&s, rate);
    synth_clock(&s, play_tick, 4661);
    played = &s;
    uint32_t hash = 0;
    for (uint32_t at = 0; at < rate * 5 / 2; at += 1000) {
        if (sides == 2)
            synth_render_stereo(&s, out, 1000);
        else
            synth_render(&s, out, 1000);
        for (int i = 0; i < 1000 * sides; i++)
            hash = hash * 31 + (uint32_t)out[i];
    }
    CHECK(headless_voice_count() < 16384); /* the log kept every voice */
    music_clear();
    return hash;
}

/* Every patch of CHARLIE's songs (tools/charliesong.txt, alim/feat/charlie-lantern) and the demos' and docs' PLAY
   strings render the samples they did before layers, at the Mac's rate in stereo and at DOS's in mono. */
TEST(synth_patches_render_as_before)
{
    static const struct {
        const char *play;
        uint32_t hash;
    } patches[] = {
        {"R2S12W1H5,4,12,5U3 T150 L8 O3 C E G > C P8 < G", 1906866798u},
        {"R2S12W4Z1,4,3,13,4H0,4,11,1U9 T150 L8 O3 C E G > C P8 < G", 4115387966u},
        {"R2W1H5,4,12,5U3 T150 L8 O3 C E G > C P8 < G", 3887838304u},
        {"R2W4Z1,4,3,13,4H0,4,11,1U9 T150 L8 O3 C E G > C P8 < G", 4272284038u},
        {"R3W4Z7,6,7,0H0,6,0,4 T150 L8 O3 C E G > C P8 < G", 2182949812u},
        {"R5W4Z3,9,3,1,3H0,4,1,2U0I0 T150 L8 O3 C E G > C P8 < G", 73426630u},
        {"R6W1H3,2,12,3U5 T150 L8 O3 C E G > C P8 < G", 2766664652u},
        {"R8W3H10,0,15,10Y2,800,8 T150 L8 O3 C E G > C P8 < G", 3084041042u},
        {"R8W4Z7,6,7,0H0,6,0,4 T150 L8 O3 C E G > C P8 < G", 3385123968u},
        {"S10R3W4Z1,4,3,13,5H3,4,11,3U5 T150 L8 O3 C E G > C P8 < G", 929588130u},
        {"S10R4W4Z1,4,3,13,4H0,4,11,1U9I0 T150 L8 O3 C E G > C P8 < G", 109058254u},
        {"S10R5W1H3,2,12,3U5 T150 L8 O3 C E G > C P8 < G", 523322242u},
        {"S10R6W0Q9Y2,1800,5H2,3,11,3U6I0 T150 L8 O3 C E G > C P8 < G", 1112452072u},
        {"S10W4Z2,8,5,3,1H0,9,5,6R5 T150 L8 O3 C E G > C P8 < G", 2660588456u},
        {"S11R3W2Y1,900,2H4,3,10,4 T150 L8 O3 C E G > C P8 < G", 3155893406u},
        {"S11R3W4Z1,4,3,13,5H3,4,11,3U5 T150 L8 O3 C E G > C P8 < G", 433967630u},
        {"S11R3W4Z3,9,3,1,3H0,4,1,2U0I0 T150 L8 O3 C E G > C P8 < G", 3725591924u},
        {"S11R5W0Q12Y2,1100,1H0,5,2,2U0I0 T150 L8 O3 C E G > C P8 < G", 1453110220u},
        {"S11R5W1H5,3,9,5 T150 L8 O3 C E G > C P8 < G", 3342871916u},
        {"S11W4Z4,7,2,0H0,6,0,3R4 T150 L8 O3 C E G > C P8 < G", 2282810188u},
        {"S5R3W0Q30H0,3,7,1 T150 L8 O3 C E G > C P8 < G", 1999196896u},
        {"S5R3W4Z1,8,3,11,4H0,4,10,1U9I2 T150 L8 O3 C E G > C P8 < G", 537238086u},
        {"S5R3W4Z2,6,3,2H0,6,0,2 T150 L8 O3 C E G > C P8 < G", 422268202u},
        {"S5R4W4Z2,5,3,1H0,5,0,2 T150 L8 O3 C E G > C P8 < G", 2941180200u},
        {"S5R5W2Y1,1600,2H0,6,0,3U0I0 T150 L8 O3 C E G > C P8 < G", 956661358u},
        {"S5W4Z4,7,2,0H0,6,0,3R4 T150 L8 O3 C E G > C P8 < G", 693556558u},
        {"S6R6W2Y1,1400,2H4,3,10,3U4 T150 L8 O3 C E G > C P8 < G", 903813282u},
        {"W0Q12H0,1,0,1 T150 L8 O3 C E G > C P8 < G", 3195572472u},
        {"W0Q25Y1,900,4H0,3,9,1 T150 L8 O3 C E G > C P8 < G", 794323188u},
        {"W0Q42H1,4,10,1U5I4 T150 L8 O3 C E G > C P8 < G", 2665004114u},
        {"W1H0,2,0,1 T150 L8 O3 C E G > C P8 < G", 3986419288u},
        {"W1H0,6,10,2 T150 L8 O3 C E G > C P8 < G", 3568144724u},
        {"W1H5,4,12,5U3 T150 L8 O3 C E G > C P8 < G", 3301799982u},
        {"W2Y1,1800,5H0,4,11,2U3I6R7 T150 L8 O3 C E G > C P8 < G", 1736106996u},
        {"W2Y1,2400,5H0,4,11,2U3I6 T150 L8 O3 C E G > C P8 < G", 4122338080u},
        {"W2Y1,2600,3H2,3,12,2U6K1,0,1R4 T150 L8 O3 C E G > C P8 < G", 557759802u},
        {"W2Y1,2800,3H2,3,12,3U2I7R6 T150 L8 O3 C E G > C P8 < G", 4118765744u},
        {"W2Y1,480,4H0,8,3,4U0I0R2 T150 L8 O3 C E G > C P8 < G", 99104454u},
        {"W3H0,2,0,1K0Y3,2500,0 T150 L8 O3 C E G > C P8 < G", 1065396974u},
        {"W3H0,3,0,1K0Y3,2500,0 T150 L8 O3 C E G > C P8 < G", 3324732178u},
        {"W3H0,3,0,2K0Y3,7000,5 T150 L8 O3 C E G > C P8 < G", 3079588002u},
        {"W3H0,5,0,1K0Y0 T150 L8 O3 C E G > C P8 < G", 2932172628u},
        {"W3H0,7,0,4K0Y3,2500,0 T150 L8 O3 C E G > C P8 < G", 2176226330u},
        {"W4H0,4,3,3Y0K1,8 T150 L8 O3 C E G > C P8 < G", 1968822198u},
        {"W4H0,5,0,3Y0K5,6 T150 L8 O3 C E G > C P8 < G", 1698602628u},
        {"W4H0,6,4,5Y0K2,10 T150 L8 O3 C E G > C P8 < G", 897676258u},
        {"W4Z1,0,0,0,0H0,1,0,1Y0K1,4 T150 L8 O3 C E G > C P8 < G", 1458034192u},
        {"W4Z1,10,4,2H0,9,0,6 T150 L8 O3 C E G > C P8 < G", 1451935662u},
        {"W4Z1,12,4,4H0,8,0,3 T150 L8 O3 C E G > C P8 < G", 1465416028u},
        {"W4Z1,2,13,0,7H3,3,1,7Y1,4775,6K3,7 T150 L8 O3 C E G > C P8 < G", 4205082258u},
        {"W4Z1,4,2,0H0,3,0,1K1,2 T150 L8 O3 C E G > C P8 < G", 1637005312u},
        {"W4Z1,4,2,0H0,4,0,2K1,3 T150 L8 O3 C E G > C P8 < G", 192907674u},
        {"W4Z1,4,3,13,4H0,4,11,1U9I0 T150 L8 O3 C E G > C P8 < G", 2415661750u},
        {"W4Z1,4,3,13,5H1,4,11,2U9I7 T150 L8 O3 C E G > C P8 < G", 888429518u},
        {"W4Z1,6,3,3,2H0,5,5,1 T150 L8 O3 C E G > C P8 < G", 41468220u},
        {"W4Z1,8,3,11,4H0,4,12,1U11I2 T150 L8 O3 C E G > C P8 < G", 62548178u},
        {"W4Z1,9,3,15,3H5,4,10,1U5I4 T150 L8 O3 C E G > C P8 < G", 3152434724u},
        {"W4Z13,3,15,13,6H0,3,0,3Y1,3793,12K1,5 T150 L8 O3 C E G > C P8 < G", 505984544u},
        {"W4Z2,15,13,14,6H1,3,0,3Y3,301,11K3,12 T150 L8 O3 C E G > C P8 < G", 1761601360u},
        {"W4Z2,8,5,3,1H0,9,5,6R5I4 T150 L8 O3 C E G > C P8 < G", 407046338u},
        {"W4Z3,9,3,1,3H0,4,1,2U0I0 T150 L8 O3 C E G > C P8 < G", 498534286u},
        {"W4Z4,2,10,9,7H0,5,0,5Y1,756,7K4,9 T150 L8 O3 C E G > C P8 < G", 2924770046u},
        {"W4Z4,6,2,0H0,6,0,3R3 T150 L8 O3 C E G > C P8 < G", 837727202u},
        {"W4Z6,3,12,0,7H3,3,0,3Y2,1900,10K3,7 T150 L8 O3 C E G > C P8 < G", 211492142u},
        {"W4Z7,6,11,14,7H0,5,0,5Y1,190,6K9,4 T150 L8 O3 C E G > C P8 < G", 1746701288u},
        {"W4Z7,6,3,0H0,8,0,6R6 T150 L8 O3 C E G > C P8 < G", 1629027160u},
        {"H0,1,0,1 Y3,6000 O6 C T150 L8 O3 C E G > C P8 < G", 3665949718u},
        {"H0,4,0,3 Y1,300,4 O1 C T150 L8 O3 C E G > C P8 < G", 2132247590u},
        {"H0,4,0,4 Y0 O4 C T150 L8 O3 C E G > C P8 < G", 3965026862u},
        {"L1 W1 V5 O3 C T150 L8 O3 C E G > C P8 < G", 4275349822u},
        {"L1 W1 V5 O3 E T150 L8 O3 C E G > C P8 < G", 2360239720u},
        {"L1 W1 V5 O3 G T150 L8 O3 C E G > C P8 < G", 2328548122u},
        {"L1 W1 V5 O4 C T150 L8 O3 C E G > C P8 < G", 391680122u},
        {"L1 W1 V5 O4 E T150 L8 O3 C E G > C P8 < G", 4006162978u},
        {"L1 W1 V5 O4 G T150 L8 O3 C E G > C P8 < G", 1873518386u},
        {"MLW0Q30Y1,2400,6H6,6,10,8V7 T150 L8 O3 C E G > C P8 < G", 3585786712u},
        {"MLW1H11,8,11,10V9 T150 L8 O3 C E G > C P8 < G", 2124528212u},
        {"MLW2Y1,900,6H9,6,10,9V8 T150 L8 O3 C E G > C P8 < G", 3909036162u},
        {"MNW0Q25H0,3,6,2V6 T150 L8 O3 C E G > C P8 < G", 1195926152u},
        {"MNW1H0,2,10,3V15 T150 L8 O3 C E G > C P8 < G", 1127799112u},
        {"MNW1H0,5,8,4V7 T150 L8 O3 C E G > C P8 < G", 2434927984u},
        {"MNW1Y1,1800,4H0,6,6,6V7 T150 L8 O3 C E G > C P8 < G", 2914438046u},
        {"MNW2Y1,1400,6H0,4,7,3V6 T150 L8 O3 C E G > C P8 < G", 2481431642u},
        {"MSW0Q40Y1,1600,4H0,3,6,2V15 T150 L8 O3 C E G > C P8 < G", 567463366u},
        {"T100 O2 L2 W4 H0,6,6,3 Z1,12,0,15 C Z1,12,4,4 C Z1,12,7,0 C T150 L8 O3 C E G > C P8 < G", 3513797790u},
        {"T100 O3 L2 W4 H0,0,15,2 Z1,6,0,15,0 C Z1,6,0,15,3 C Z1,6,0,15,5 C T150 L8 O3 C E G > C P8 < G", 140180448u},
        {"T100 O4 L4 W0 Q30 H1,4,10,2 K0 E G A2 K8,0,2 E G A2 T150 L8 O3 C E G > C P8 < G", 226080516u},
        {"T100 O4 L4 W4 Z1,9,3,11,4 H0,4,12,1 R0 C E G P4 R6 C E G P4 R12 C E G P2 T150 L8 O3 C E G > C P8 < G", 1154081732u},
        {"T100 O4 W4 Z1,9,3,11,4 H0,4,12,2 L8 C D E F U0 G1 U6 G1 U12 G1 T150 L8 O3 C E G > C P8 < G", 1723135158u},
        {"T120 L4 W4 Z1,0 H0,4,3,3 K1,8 V12 O1 C C C C T150 L8 O3 C E G > C P8 < G", 3271896250u},
        {"T120 O2 L4 W2 Y1,3000 C Y1,1200 C Y1,500 C Y1,200 C T150 L8 O3 C E G > C P8 < G", 3285793354u},
        {"T120 O3 L2 W4 H0,0,15,2 Z1,0 C Z1,4 C Z1,8 C Z1,12 C T150 L8 O3 C E G > C P8 < G", 1116666174u},
        {"T120 O3 L2 W4 H0,0,15,2 Z1,8 C Z2,8 C Z3,8 C Z7,8 C T150 L8 O3 C E G > C P8 < G", 981803054u},
        {"T120 O3 L4 W0 C E G P4 W1 C E G P4 W2 C E G P4 W3 C E G", 2304395852u},
        {"T120 O3 L4 W2 Y1,2000,4 H0,4,12,2 K0 C G > C < G K8 C G > C < G T150 L8 O3 C E G > C P8 < G", 386042140u},
        {"T120 O4 L4 W0 Q30 H0,4,9,2 S1 C S4 E S8 G S12 > C S15 E T150 L8 O3 C E G > C P8 < G", 1029942890u},
        {"T120MLW1H2,6,11,5V12 T150 L8 O3 C E G > C P8 < G", 237704018u},
        {"T120MNW2Y1,500,6H0,7,8,4V8 T150 L8 O3 C E G > C P8 < G", 1629317448u},
        {"T120MSW0Q30Y0H0,4,5,3V5 T150 L8 O3 C E G > C P8 < G", 1646805084u},
        {"T130 L1 W0 Q30 H4,0,12,6 V7 O3 E E F G T150 L8 O3 C E G > C P8 < G", 2408830968u},
        {"T130 L1 W0 Q30 H4,0,12,6 V7 O3 G A A B T150 L8 O3 C E G > C P8 < G", 86963572u},
        {"T130 L1 W0 Q30 H4,0,12,6 V7 O4 C C C D T150 L8 O3 C E G > C P8 < G", 788372870u},
        {"T130 L16 W0 Q20 H0,2,8,1 V6 O3 C E G > C < G E C E G > C < G E C E G E", 670218450u},
        {"T130 L8 W3 V7 T150 L8 O3 C E G > C P8 < G", 2544206864u},
        {"T130 L8 W3 V8 T150 L8 O3 C E G > C P8 < G", 2103306738u},
        {"T130 O2 L2 W1 H0,8,10,4 V11 C C O1 A A O2 F F G G T150 L8 O3 C E G > C P8 < G", 691032668u},
        {"T130 O2 L2 W1 V10 C C T150 L8 O3 C E G > C P8 < G", 659046806u},
        {"T130 O4 L8 W2 H0,5,9,3 V8 E G > C < G E G E C T150 L8 O3 C E G > C P8 < G", 2697988030u},
        {"T140 L8 W4 Z1,0 H0,4,3,3 K1,8 V12 O1 C W3 K0 H0,3,0,1 Y3,2500 O6 B W4 Y0 H0,4,3,3 K1,8 O1 C W3 K0 H0,3,0,1 Y3,2500 O6 B T150 L8 O3 C E G > C P8 < G", 844128392u},
        {"T140 W4 Z1,12,4,4 H0,5,5,1 V11 L4 O2 C G C G T150 L8 O3 C E G > C P8 < G", 196505004u},
        {"T140 W4 Z1,9,3,11,4 H0,4,12,1 U8 S10 R4 V11 L8 O4 C E G > C < G E C4 T150 L8 O3 C E G > C P8 < G", 3195479784u},
        {"T140MLW2Y1,2600,3H1,6,11,5V11 T150 L8 O3 C E G > C P8 < G", 2775734516u},
        {"T140MNW0Q30H0,4,6,3V6 T150 L8 O3 C E G > C P8 < G", 1894971010u},
        {"T140MNW1V12L16O4CO3GECL8EGL4O4C T150 L8 O3 C E G > C P8 < G", 1050245326u},
        {"T140MNW2V9L4O2CO1GO2C T150 L8 O3 C E G > C P8 < G", 1409339754u},
        {"T140MNW2Y1,700,5H0,8,9,5V13 T150 L8 O3 C E G > C P8 < G", 1250991570u},
        {"T150MLW1H1,5,10,4V12 T150 L8 O3 C E G > C P8 < G", 3602581064u},
        {"T150MLW2V12L32O2CO1GECL8O0G T150 L8 O3 C E G > C P8 < G", 354779788u},
        {"T150MNW2Y1,700,6H0,6,8,3V9 T150 L8 O3 C E G > C P8 < G", 1490281248u},
        {"T150MSW0Q30H0,3,5,2V5 T150 L8 O3 C E G > C P8 < G", 3166724424u},
        {"T150W3H0,9,0,8V12L4O1C T150 L8 O3 C E G > C P8 < G", 2219896380u},
        {"T160MNW0Q25H0,6,10,3V11 T150 L8 O3 C E G > C P8 < G", 3485589524u},
        {"T160MNW0Q25V12L16O4CEGO5L8CO4L16GO5L4C T150 L8 O3 C E G > C P8 < G", 2133084668u},
        {"T160MNW1H0,3,8,4V11L64O5CEGO6CL16E T150 L8 O3 C E G > C P8 < G", 566410512u},
        {"T160MNW2V9L4O2CL8GL4O3C T150 L8 O3 C E G > C P8 < G", 3245553662u},
        {"T160MNW2Y1,900,9H0,3,9,2V7 T150 L8 O3 C E G > C P8 < G", 486388916u},
        {"T160MNW3Y0H0,2,0,1 T150 L8 O3 C E G > C P8 < G", 3230823732u},
        {"T180W0Q50V9L16O4AO3AO4AO3A T150 L8 O3 C E G > C P8 < G", 4083084726u},
        {"T200W0Q50V6L64O5G T150 L8 O3 C E G > C P8 < G", 2342643192u},
        {"T200W0Q50V7L32O4CEGO5C T150 L8 O3 C E G > C P8 < G", 1770335506u},
        {"T200W0Q50V8L16O2CO1G T150 L8 O3 C E G > C P8 < G", 514368088u},
        {"T200W1V7L64O6A# T150 L8 O3 C E G > C P8 < G", 1758798740u},
        {"T200W3Y2,2500,10H0,5,0,4V8L16O4C T150 L8 O3 C E G > C P8 < G", 1541662938u},
        {"T255 L16 W0 Q25 H0,3,0,1 V10 O3 C E G > C E T150 L8 O3 C E G > C P8 < G", 91377756u},
        {"T255 L16 W1 H0,2,12,2 V11 O3 C E G > C E G > C E G T150 L8 O3 C E G > C P8 < G", 3344549866u},
        {"T255 L16 W2 H0,3,0,1 V10 N76 N70 N64 N58 N52 N46 T150 L8 O3 C E G > C P8 < G", 1579637200u},
        {"T255 W0 Q50 H0,6,0,6 V10 O5 L16 B > E4 T150 L8 O3 C E G > C P8 < G", 1347151172u},
        {"T255 W3 H0,10,0,10 V15 Y1,900,6 O1 C4 P4 T150 L8 O3 C E G > C P8 < G", 1706538650u},
        {"T255 W3 H0,3,0,2 V12 O4 C16 P8 T150 L8 O3 C E G > C P8 < G", 1302225380u},
        {"T255W1V13Y0K0I0H0,0,15,0 T150 L8 O3 C E G > C P8 < G", 1094925068u},
        {"T255W3Y3,3000,4H0,2,0,2V6L32O5C T150 L8 O3 C E G > C P8 < G", 489726166u},
        {"T80MLW1H3,8,9,6V11 T150 L8 O3 C E G > C P8 < G", 654820112u},
        {"T80MNW2Y1,400,8H0,8,8,5V9 T150 L8 O3 C E G > C P8 < G", 1729135488u},
        {"T80MSW0Q25H0,5,4,4V4 T150 L8 O3 C E G > C P8 < G", 3520270662u},
        {"T90 O3 L1 W2 Y1,2400,4 H3,5,11,4 I0 C I4 C I8 C T150 L8 O3 C E G > C P8 < G", 1666710152u},
        {"T90 O3 L2 W2 H0,0,15,0 C H0,6,0,4 C H10,0,15,8 C W1 H0,9,0,9 > C T150 L8 O3 C E G > C P8 < G", 1681422000u},
        {"T90 W0 Q30 H2,6,8,8 V10 O3 L8 G F# F E2 T150 L8 O3 C E G > C P8 < G", 3161249698u},
        {"T96MLW1H2,6,11,5V11 T150 L8 O3 C E G > C P8 < G", 353704050u},
        {"T96MNW2Y1,500,6H0,7,8,4V9 T150 L8 O3 C E G > C P8 < G", 1555659666u},
        {"T96MSW0Q30H0,4,5,3V4 T150 L8 O3 C E G > C P8 < G", 784933188u},
        {"W0 L8 Q50 C C Q25 C C Q10 C C Q5 C C T150 L8 O3 C E G > C P8 < G", 2027069648u},
        {"W0Q50H0,6,0,4V14O1G32C32P16W0Q50H0,6,0,4V14O1G32C32P16 T150 L8 O3 C E G > C P8 < G", 2618006094u},
        {"W0Q50H0,6,0,4V14O1G32C32P16W3H0,1,0,1V5O6C32P32P16 T150 L8 O3 C E G > C P8 < G", 495555150u},
        {"W2H0,2,0,2V7L64O6CB T150 L8 O3 C E G > C P8 < G", 945833432u},
        {"W2I15Y2,1000,6H0,6,2,3K4,0,6L8O3D T150 L8 O3 C E G > C P8 < G", 3947070608u},
        {"W2I15Y2,1300,6H0,5,3,3K3,0,7L16O3FP32L8O3D T150 L8 O3 C E G > C P8 < G", 2072763832u},
        {"W2L32O6ECP32EC T150 L8 O3 C E G > C P8 < G", 619277550u},
        {"W3H0,1,0,1V5L64O6C T150 L8 O3 C E G > C P8 < G", 4105853194u},
        {"W3H0,3,0,1Y3,2500K0O6B T150 L8 O3 C E G > C P8 < G", 383347482u},
        {"W3H0,4,0,3V9L32O3C T150 L8 O3 C E G > C P8 < G", 4161414372u},
        {"W3H0,5,0,1Y0K0O6C T150 L8 O3 C E G > C P8 < G", 1212410784u},
        {"W3H0,5,0,4V12O4C16P16W3H0,1,0,1V5O6C32P32P16 T150 L8 O3 C E G > C P8 < G", 1891521500u},
        {"W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16W3H0,5,0,4V12O4C16W3H0,5,0,4V9O4C16 T150 L8 O3 C E G > C P8 < G", 529220428u},
        {"W3H0,7,0,4Y3,2500K0O6B T150 L8 O3 C E G > C P8 < G", 2588635310u},
        {"W3H0,9,0,8V12L8O2C T150 L8 O3 C E G > C P8 < G", 2242777880u},
        {"W3L32O2CL16O1C T150 L8 O3 C E G > C P8 < G", 1677692026u},
        {"W3L32O5CO3GL16O2C T150 L8 O3 C E G > C P8 < G", 3718372590u},
        {"W3Y3,3500,0H3,8,0,3L4O6B T150 L8 O3 C E G > C P8 < G", 1978461094u},
        {"W4 Z1,12,4,4 H0,5,5,1 T150 L8 O3 C E G > C P8 < G", 1061079860u},
        {"W4 Z1,12,4,4 H0,5,5,1 V11 MN L8 T150 L8 O3 C E G > C P8 < G", 344504618u},
        {"W4 Z1,4,3,13,4 H0,4,11,1 T150 L8 O3 C E G > C P8 < G", 2150017718u},
        {"W4 Z1,4,3,13,4 H0,4,11,1 U9 R2 V7 MS L8 T150 L8 O3 C E G > C P8 < G", 1682123826u},
        {"W4 Z1,9,3,11,4 H0,4,12,1 T150 L8 O3 C E G > C P8 < G", 270129488u},
        {"W4 Z1,9,3,11,4 H0,4,12,1 U8 R4 S8 V11 ML L8 T150 L8 O3 C E G > C P8 < G", 3392519178u},
        {"W4 Z7,6,7,0 H0,9,0,9 T150 L8 O3 C E G > C P8 < G", 1551889634u},
        {"W4Z1,0H0,4,3,3Y0K1,8O1C T150 L8 O3 C E G > C P8 < G", 2568337190u},
        {"W4Z1,0H0,5,0,3Y0K5,6O2G T150 L8 O3 C E G > C P8 < G", 3176789556u},
        {"Y1,800,0 C Y1,800,8 C Y1,800,15 C Y0 V15 C V7 C V3 C T150 L8 O3 C E G > C P8 < G", 682781819u},
        {"Z1,0L16 T150 L8 O3 C E G > C P8 < G", 1956616212u},
        /* docs/sound.md's example: three voices, placed and in the room */
        {"T140 W4 Z1,9,3,11,4 H0,4,12,1 U8 S10 R4 V11 L8 O4 C E G > C < G E C4\", "
         "\"T140 W4 Z1,12,4,4 H0,5,5,1 V11 L4 O2 C G C G\", \"T140 L8 W4 Z1,0 H0,4,3,3 K1,8 V12 O1 C W3 K0 H0,3,0,1 "
         "Y3,2500 O6 B W4 Y0 H0,4,3,3 K1,8 O1 C W3 K0 H0,3,0,1 Y3,2500 O6 B", 343417320u},
    };

    qbrt_realtime(false);
    for (size_t i = 0; i < sizeof patches / sizeof patches[0]; i++) {
        const uint32_t hash = played_hash(S, patches[i].play, 44100, 2) * 31 + played_hash(S, patches[i].play, 15873, 1);
        CHECK(hash == patches[i].hash);
    }
    qbrt_realtime(true);
}
