/* The synthesizer (synth.h). Integer throughout, so every build renders the same samples. The filter is a Chamberlin
   state-variable one, run twice a sample so it stays stable up to its highest cutoff. What a note needs is worked out
   when its voice is set; a sample does no division. */
#include "plat/synth/synth.h"

enum {
    /* the speaker: QB's square wave, faded in and out over 2 ms so it does not click */
    SPEAKER_LOUD = 4000,
    FADES_A_SECOND = 500,
    /* a voice at volume 15 */
    VOICE_LOUD = 6000,
    HIGHEST_NIBBLE = 15,
    FULL = 1 << 24,
    ONE = 1 << 16,      /* the filter's fixed point */
    STATE_SHIFT = 8,    /* the filter's state, in 256ths of a sample */
    PI_ONES = 205887,   /* pi * ONE */
    DAMPING_MOST = 91750, /* 1.4: resonance 0 */
    DAMPING_STEP = 5680,  /* 1.3 / 15 */
    SYNTH_BLOCK = 64,     /* samples a voice renders at a time */
    SINE_SHIFT = 20,      /* a phase's top bits index the sine */
    FEEDBACK_SHIFT = 9,   /* feedback 1 bends by pi / 16 at full, each more twice that: the sum of two samples */
    VIBRATO_HERTZ = 6,
    VIBRATO_WAITS = 5,    /* a fifth of a second before it starts */
    VIBRATO_SHIFT = 24,   /* depth 15 is 3% of the pitch */
    LOUDEST = 32767,
    GAIN_SHIFT = 12,      /* the limiter's gain is FULL >> GAIN_SHIFT: 2^12 is 1 */
    RELEASES_A_SECOND = 5, /* a fifth of a second from silence back to full */
    ROOM_FEEDBACK = 200,  /* of 256: a comb's sound comes back at 0.78, a tail of about a second */
    ROOM_TAIL = 3,        /* seconds until the tail is nothing */
    FM_DEPTH = 10431,     /* a depth's half radian of phase, for each of a modulator's 32767: 2^32 / (4 pi 32767) */
};

enum Stage { STAGE_ATTACK, STAGE_DECAY, STAGE_RELEASE, STAGE_CUT };

/* SID's times from silence to full (attack) and from full to silence (decay, release), ms */
static const uint16_t attack_ms[16] = {2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000};
static const uint16_t glide_ms[16] = {0, 10, 20, 30, 40, 60, 80, 100, 120, 150, 200, 250, 300, 400, 500, 800};
/* 2^(-n/12) of 65536: a pitch n semitones under */
static const uint32_t under[25] = {65536, 61858, 58386, 55109, 52016, 49097, 46341, 43740, 41285,
                                   38968, 36781, 34716, 32768, 30929, 29193, 27554, 26008, 24548,
                                   23170, 21870, 20643, 19484, 18390, 17358, 16384};
static const uint16_t decay_ms[16] = {6, 24, 48, 72, 114, 168, 204, 240, 300, 750, 1500, 2400, 3000, 9000, 15000, 24000};
/* SID's exponential fall: below each level (of 255) the step is divided by its divisor */
static const int32_t below[SYNTH_CURVE] = {256, 93, 54, 26, 14, 6};
static const int32_t divisor[SYNTH_CURVE] = {1, 2, 4, 8, 16, 30};

/* 32767 sin(k pi / (SYNTH_SINE / 2)), k to a quarter of SYNTH_SINE: the series to x^7, ONE being 1. */
static int32_t quarter_sin(const int32_t k)
{
    const int32_t x = k * PI_ONES / (SYNTH_SINE / 2);
    const int32_t x2 = (int32_t)((int64_t)x * x >> 16);
    int32_t t = ONE - x2 / 42;

    t = ONE - (int32_t)((int64_t)x2 * t >> 16) / 20;
    t = ONE - (int32_t)((int64_t)x2 * t >> 16) / 6;
    return (int32_t)((int64_t)x * t >> 16) * 32767 >> 16;
}

void synth_init(Synth *const s, const uint32_t rate)
{
    *s = (Synth){.rate = rate, .per_hertz = UINT32_MAX / rate, .rest = (UINT32_MAX % rate + 1) % rate,
                 .release = (int32_t)((uint32_t)FULL * RELEASES_A_SECOND / rate)};
    for (int i = 0; i < SYNTH_SINE / 2; i++) {
        const int32_t k = i & (SYNTH_SINE / 4 - 1);
        const int32_t y = quarter_sin(i < SYNTH_SINE / 4 ? k : SYNTH_SINE / 4 - k);

        s->sine[i] = (int16_t)y;
        s->sine[i + SYNTH_SINE / 2] = (int16_t)-y;
    }
    /* the combs' and all-passes' lengths, tenths of a millisecond: none a multiple of another */
    static const uint16_t comb[SYNTH_COMBS] = {297, 371, 411, 437}, pass[SYNTH_PASSES] = {50, 17};

    for (int i = 0; i < SYNTH_COMBS; i++) {
        const uint32_t len = rate * comb[i] / 10000;

        s->room.comb_len[i] = (uint16_t)(len < 1 ? 1 : len > SYNTH_COMB_MOST ? SYNTH_COMB_MOST : len);
    }
    for (int i = 0; i < SYNTH_PASSES; i++) {
        const uint32_t len = rate * pass[i] / 10000;

        s->room.pass_len[i] = (uint16_t)(len < 1 ? 1 : len > SYNTH_PASS_MOST ? SYNTH_PASS_MOST : len);
    }
    for (int i = 0; i < DEV_VOICES; i++) {
        s->osc[i].noise = 0x7FFFF8;
        s->osc[i].stage = STAGE_CUT;
    }
}

void synth_clock(Synth *const s, void (*const tick)(void), const uint32_t sixteenths)
{
    s->tick_step = (s->rate << 16) / sixteenths << 4; /* rate under 2^16 */
    s->tick_left = 0;
    s->tick = tick;
}

void synth_tone(Synth *const s, const uint32_t hertz)
{
    __atomic_store_n(&s->want, hertz, __ATOMIC_RELAXED);
}

/* The step a sample of `ms` from silence to full takes. */
static int32_t step_of(const Synth *const s, const uint32_t ms)
{
    const int32_t samples = (int32_t)(ms * s->rate / 1000);
    return FULL / (samples > 0 ? samples : 1);
}

static void falling(const Synth *const s, int32_t *const steps, const uint32_t ms)
{
    const int32_t step = step_of(s, ms);

    for (int i = 0; i < SYNTH_CURVE; i++)
        steps[i] = step / divisor[i] > 0 ? step / divisor[i] : 1;
}

/* 2 sin(x) for x from 0 to pi / 12, ONE being 1 */
static int32_t twice_sin(const int32_t x)
{
    const int32_t x2 = x * x >> 16;
    const int32_t inner = ONE - x2 / 20;
    return (x * (ONE - (x2 * inner >> 16) / 6) >> 16) * 2;
}

/* A note begins: the modulator at full, the vibrato waiting, the pitch where its slide starts. */
static void begin(const Synth *const s, SynthOsc *const o, const DevVoice *const v)
{
    const uint32_t samples = glide_ms[v->glide & HIGHEST_NIBBLE] * s->rate / 1000;
    const uint64_t above = (uint64_t)o->step + (uint64_t)(o->step / 4) * v->from;
    const uint32_t over = !v->from ? o->at : above < INT32_MAX ? (uint32_t)above : INT32_MAX;
    const uint32_t from =
        v->under ? (uint32_t)((uint64_t)o->step * under[v->under < 24 ? v->under : 24] >> 16) : over;

    o->bend = FULL;
    o->fed[0] = o->fed[1] = o->fed2[0] = o->fed2[1] = 0;
    o->age = 0;
    o->lfo = 0;
    o->at = samples && from ? from : o->step;
    o->slide = samples ? (int32_t)(o->step - o->at) / (int32_t)samples : 0;
    if (o->at != o->step && !o->slide)
        o->slide = o->step > o->at ? 1 : -1;
}

void synth_voice(Synth *const s, const unsigned voice, const DevVoice *const v)
{
    if (voice >= DEV_VOICES)
        return;
    SynthOsc *const o = &s->osc[voice];
    const bool attack = v->gate && (!o->set.gate || v->trigger != o->set.trigger);

    o->set = *v;
    o->step = v->hertz * s->per_hertz + v->hertz * s->rest / s->rate; /* hertz * 2^32 / rate */
    if (attack && v->hertz)
        begin(s, o, v);
    if (!v->hertz)
        o->stage = STAGE_CUT;
    else if (attack)
        o->stage = STAGE_ATTACK;
    else if (!v->gate)
        o->stage = STAGE_RELEASE;

    o->attack = step_of(s, attack_ms[v->envelope >> 12 & HIGHEST_NIBBLE]);
    falling(s, o->decay, decay_ms[v->envelope >> 8 & HIGHEST_NIBBLE]);
    falling(s, o->release, decay_ms[v->envelope & HIGHEST_NIBBLE]);
    o->gain = (int32_t)(v->volume * (uint32_t)VOICE_LOUD * 32768u / (HIGHEST_NIBBLE * 32767u));

    /* twice a sample: up to a sixth of the rate */
    const uint32_t highest = s->rate / 6, cutoff = v->cutoff < highest ? v->cutoff : highest;
    o->f = twice_sin((int32_t)(PI_ONES * cutoff / (2 * s->rate)));
    o->damping = DAMPING_MOST - v->resonance * DAMPING_STEP;
    o->ratio = v->ratio ? v->ratio : 1;
    o->depth = v->depth * (uint32_t)FM_DEPTH;
    o->bend_step = step_of(s, decay_ms[v->fall >> 4]);
    o->bend_floor = (v->fall & HIGHEST_NIBBLE) * (FULL / HIGHEST_NIBBLE);
    o->feedback = v->feedback ? 1u << (FEEDBACK_SHIFT + v->feedback) : 0;
    o->lfo_step = VIBRATO_HERTZ * s->per_hertz;
}

/* The speaker's next sample, as QB's square wave always was. */
static int32_t speaker(Synth *const s)
{
    const uint32_t want = __atomic_load_n(&s->want, __ATOMIC_RELAXED);
    const int32_t fade = (int32_t)(s->rate / FADES_A_SECOND);

    if (!want && !s->hertz && !s->gain) /* silent: nothing changes */
        return 0;
    if (want != s->hertz && s->gain == 0) { /* a new tone starts from silence */
        s->hertz = want;
        s->phase = 0;
    }
    s->gain += want == s->hertz && s->hertz ? s->gain < fade : -(s->gain > 0);
    s->phase = (s->phase + s->hertz) % s->rate;
    return (s->phase < s->rate / 2 ? SPEAKER_LOUD : -SPEAKER_LOUD) * s->gain / fade;
}

/* A falling envelope's step at its level. */
static int32_t fall(const SynthOsc *const o, const int32_t *const steps)
{
    const int32_t level = o->level >> 16; /* 0 to 255, as SID's */
    int i = SYNTH_CURVE - 1;

    while (i > 0 && level >= below[i])
        i--;
    return steps[i];
}

[[gnu::always_inline]] static inline void envelope(const Synth *const s, SynthOsc *const o)
{
    const int32_t sustain = (int32_t)(o->set.envelope >> 4 & HIGHEST_NIBBLE) * (FULL / HIGHEST_NIBBLE);

    switch (o->stage) {
    case STAGE_ATTACK:
        o->level += o->attack;
        if (o->level >= FULL) {
            o->level = FULL;
            o->stage = STAGE_DECAY;
        }
        break;
    case STAGE_DECAY:
        if (o->level > sustain) {
            o->level -= fall(o, o->decay);
            if (o->level < sustain)
                o->level = sustain;
        }
        break;
    case STAGE_RELEASE:
        o->level -= fall(o, o->release);
        break;
    default:
        o->level -= FULL / (int32_t)(s->rate / FADES_A_SECOND);
    }
    if (o->level < 0)
        o->level = 0;
}

/* The band of levels fall()'s step holds over: the lowest level in it. */
static int32_t fall_floor(const int32_t level)
{
    const int32_t at = level >> 16;
    int i = SYNTH_CURVE - 1;

    while (i > 0 && at >= below[i])
        i--;
    return i < SYNTH_CURVE - 1 ? below[i + 1] << 16 : 0;
}

/* How many of the next `left` samples the envelope moves by one step, *delta, with no stage changed and nothing
   clipped: each as envelope() would. 0: the next is envelope()'s own. */
static int envelope_run(const Synth *const s, const SynthOsc *const o, const int left, int32_t *const delta)
{
    const int32_t sustain = (int32_t)(o->set.envelope >> 4 & HIGHEST_NIBBLE) * (FULL / HIGHEST_NIBBLE);
    const int32_t level = o->level;
    int32_t step, floor, k;

    switch (o->stage) {
    case STAGE_ATTACK:
        *delta = o->attack;
        k = (FULL - 1 - level) / o->attack; /* each stays under FULL */
        return k < left ? (int)k : left;
    case STAGE_DECAY:
        if (level <= sustain) {
            *delta = 0;
            return left;
        }
        step = fall(o, o->decay);
        floor = sustain;
        break;
    case STAGE_RELEASE:
        step = fall(o, o->release);
        floor = 0;
        break;
    default:
        step = FULL / (int32_t)(s->rate / FADES_A_SECOND);
        *delta = -step;
        k = level / step;
        return k < left ? (int)k : left;
    }
    *delta = -step;
    k = (level - floor) / step;                      /* each stays at the floor or over */
    const int32_t band = (level - fall_floor(level)) / step + 1; /* each starts in the step's band */
    k = band < k ? band : k;
    return k < left ? (int)k : left;
}

#if defined(__i386__) && defined(_WIN32) /* DOS: voice_run's samples by plat/dos32/synthrun.asm */
#define SYNTH_ASM 1
#else
/* What rounds a jump up of the whole height, `past` the phase since it (PolyBLEP): a jump between two samples is
   spread over them, and its harmonics over half the rate, which came back down as tones off the note, mostly go.
   Nothing but in the sample each side of the jump. */
[[gnu::always_inline]] static inline int32_t edge(const uint32_t past, const uint32_t step)
{
    const uint32_t before = 0u - past, part = step >> 16;

    if (past >= step && before > step)
        return 0;
    if (!part)
        return 0;
    const int32_t t = (int32_t)(((past < step ? past : before) >> 16 << 15) / part);
    const int32_t left = t < 32768 ? 32768 - t : 0;

    return past < step ? -(left * left >> 15) : left * left >> 15;
}

/* The wave's next sample, -32767 to 32767. */
[[gnu::always_inline]] static inline int32_t wave(SynthOsc *const o, const int16_t *const sine)
{
    const uint32_t was = o->phase;

    o->phase += o->step;
    switch (o->set.wave) {
    case DEV_TRIANGLE: {
        const int32_t x = (int32_t)(o->phase >> 16);
        return (x < 32768 ? x * 2 : (65535 - x) * 2) - 32767;
    }
    case DEV_SAW:
        return (int32_t)(o->phase >> 16) - 32768 - edge(o->phase, o->step);
    case DEV_NOISE:
        /* the register steps 16 times a period, as SID's on bit 19 of its 24 */
        if (~was & o->phase & 1u << 27) {
            const uint32_t bit = (o->noise >> 22 ^ o->noise >> 17) & 1;
            o->noise = (o->noise << 1 | bit) & 0x7FFFFF;
        }
        return (int32_t)(o->noise >> 15 & 0xFF) * 257 - 32767;
    case DEV_FM: {
        /* a sine bent by a sine, which bends itself: the bend falls from full, as a plucked string's brightness */
        const uint32_t self = (uint32_t)(o->fed[0] + o->fed[1]) * o->feedback;
        const int32_t bend = sine[(o->phase * o->ratio + self) >> SINE_SHIFT] * (o->bend >> 9) >> 15;

        o->fed[1] = o->fed[0];
        o->fed[0] = bend;
        o->bend -= o->slope;
        return sine[(o->phase + (uint32_t)bend * o->depth) >> SINE_SHIFT];
    }
    default:
        return (o->phase >> 20 < o->set.width ? 32767 : -32767) + edge(o->phase, o->step) -
               edge(o->phase - ((uint32_t)o->set.width << 20), o->step);
    }
}

/* The wave's next sample, the mean of two a little apart in pitch if the voice is a unison: noise is not. */
[[gnu::always_inline]] static inline int32_t voiced(SynthOsc *const o, const int16_t *const sine)
{
    const int32_t one = wave(o, sine);

    if (!o->set.spread || o->set.wave == DEV_NOISE)
        return one;
    const SynthOsc first = *o;

    o->phase = o->phase2;
    o->step = o->step2;
    o->fed[0] = o->fed2[0];
    o->fed[1] = o->fed2[1];
    o->bend = first.bend + first.slope; /* the modulator's level this sample, not the next */
    const int32_t two = wave(o, sine);

    o->phase2 = o->phase;
    o->fed2[0] = o->fed[0];
    o->fed2[1] = o->fed[1];
    o->phase = first.phase;
    o->step = first.step;
    o->fed[0] = first.fed[0];
    o->fed[1] = first.fed[1];
    o->bend = first.bend;
    return (one + two) >> 1;
}

[[gnu::always_inline]] static inline int32_t filtered(SynthOsc *const o, const int32_t sample)
{
    const int32_t in = sample * (1 << STATE_SHIFT);
    int32_t high = 0;

    for (int i = 0; i < 2; i++) {
        o->low += (int32_t)((int64_t)o->f * o->band >> 16);
        high = in - o->low - (int32_t)((int64_t)o->damping * o->band >> 16);
        o->band += (int32_t)((int64_t)o->f * high >> 16);
    }
    switch (o->set.filter) {
    case DEV_LOW_PASS:
        return o->low >> STATE_SHIFT;
    case DEV_BAND_PASS:
        return o->band >> STATE_SHIFT;
    default:
        return high >> STATE_SHIFT;
    }
}
#endif

/* The pitch for the next `run` samples, in o->step: the slide moved on towards `target`, the vibrato added. Both
   step once a run, not a sample: a run is 64 samples at most. */
[[gnu::always_inline]] static inline int pitch_run(SynthOsc *const o, const uint32_t rate, const uint32_t target,
                                                  const int run)
{
    if (o->at != target) {
        const int64_t next = (int64_t)o->at + (int64_t)o->slide * run;

        o->at = !o->slide || (o->slide > 0 ? next >= target : next <= target) ? target : (uint32_t)next;
    }
    o->step = o->at;
    if (o->set.vibrato && o->age >= rate / VIBRATO_WAITS) {
        const int32_t x = (int32_t)(o->lfo >> 16);
        const int32_t sway = x < 16384 ? x * 2 : x < 49152 ? 65535 - x * 2 : x * 2 - 131071; /* a triangle from 0 */

        o->step += (uint32_t)(int32_t)((int64_t)o->at * (o->set.vibrato * sway) >> VIBRATO_SHIFT);
        o->lfo += o->lfo_step * (uint32_t)run;
    } else
        o->age += (uint32_t)run;
    o->step2 = o->step + ((o->step >> 10) + (o->step >> 13)) * o->set.spread; /* 0.11% is 2 cents */
    return run;
}

/* How many of the next `run` samples FM's modulator falls by one step, o->slope, and stays over its floor. */
[[gnu::always_inline]] static inline int bend_run(SynthOsc *const o, const int run)
{
    const int32_t left = (o->bend - o->bend_floor) / o->bend_step;

    if (left <= 0) {
        o->bend = o->bend_floor;
        o->slope = 0;
        return run;
    }
    o->slope = o->bend_step;
    return left < run ? (int)left : run;
}

#ifdef SYNTH_ASM
typedef struct SynthRun {
    uint32_t phase, step;
    int32_t level, delta, low, band, f, damping, gain;
    int32_t *sum;
    int32_t n;
    uint32_t noise, width, kind, in;
    const int16_t *sine;
    uint32_t ratio, depth;
    int32_t bend, slope, fed[2];
    uint32_t feedback;
} SynthRun;
void synth_run(SynthRun *r);
#endif

/* Voice o's next n samples added to sum, its wave and filter fixed: the voice's state in locals the loop keeps in
   registers. Returns early once the voice is silent until its next note. */
[[gnu::always_inline]] static inline void voice_run(const Synth *const s, SynthOsc *const into, int32_t *const sum,
                                                    const int n, const unsigned wave_kind, const unsigned filter)
{
    SynthOsc o = *into;

    o.set.wave = (unsigned char)wave_kind;
    o.set.filter = (unsigned char)filter;
    for (int i = 0; i < n;) {
        if (o.level == 0 && (o.stage == STAGE_CUT || o.stage == STAGE_RELEASE)) /* silent until the next note */
            break;
        int32_t delta;
        int run = envelope_run(s, &o, n - i, &delta);

        if (!run) {
            envelope(s, &o);
            run = 1;
            delta = 0;
        } else
            o.level += delta;
        run = pitch_run(&o, s->rate, into->step, run);
        if (wave_kind == DEV_FM)
            run = bend_run(&o, run);
#ifdef SYNTH_ASM
        SynthRun r = {o.phase, o.step, o.level, delta, o.low, o.band, o.f, o.damping, o.gain, sum + i, run, o.noise,
                      o.set.width, wave_kind * 4 + filter, 0, s->sine, o.ratio, o.depth,
                      o.bend, o.slope, {o.fed[0], o.fed[1]}, o.feedback};
        synth_run(&r);
        o.phase = r.phase;
        o.noise = r.noise;
        o.level = r.level;
        o.low = r.low;
        o.band = r.band;
        o.bend = r.bend;
        o.fed[0] = r.fed[0];
        o.fed[1] = r.fed[1];
        i += run;
#else
        for (const int end = i + run;;) {
            /* under 2^15 times 2^15, and unfiltered under 2^15 times the gain: int32 holds them */
            const int32_t out = voiced(&o, s->sine) * (o.level >> 9) >> 15;
            if (filter != DEV_UNFILTERED)
                sum[i] += (int32_t)((int64_t)filtered(&o, out) * o.gain >> 15);
            else
                sum[i] += out * o.gain >> 15;
            if (++i == end)
                break;
            o.level += delta;
        }
#endif
    }
    into->phase = o.phase;
    into->noise = o.noise;
    into->level = o.level;
    into->stage = o.stage;
    into->low = o.low;
    into->band = o.band;
    into->bend = o.bend;
    into->fed[0] = o.fed[0];
    into->fed[1] = o.fed[1];
    into->at = o.at;
    into->phase2 = o.phase2;
    into->fed2[0] = o.fed2[0];
    into->fed2[1] = o.fed2[1];
    into->lfo = o.lfo;
    into->age = o.age;
}

#define VOICE_FILTERS(w)                                                                                               \
    case DEV_UNFILTERED: voice_run(s, o, sum, n, w, DEV_UNFILTERED); break;                                           \
    case DEV_LOW_PASS: voice_run(s, o, sum, n, w, DEV_LOW_PASS); break;                                               \
    case DEV_BAND_PASS: voice_run(s, o, sum, n, w, DEV_BAND_PASS); break;                                             \
    default: voice_run(s, o, sum, n, w, DEV_HIGH_PASS);

/* Each wave and filter its own loop. */
static void voice(const Synth *const s, SynthOsc *const o, int32_t *const sum, const int n)
{
    if (o->level == 0 && (o->stage == STAGE_CUT || o->stage == STAGE_RELEASE))
        return;
    switch (o->set.wave) {
    case DEV_TRIANGLE:
        switch (o->set.filter) { VOICE_FILTERS(DEV_TRIANGLE) }
        break;
    case DEV_SAW:
        switch (o->set.filter) { VOICE_FILTERS(DEV_SAW) }
        break;
    case DEV_NOISE:
        switch (o->set.filter) { VOICE_FILTERS(DEV_NOISE) }
        break;
    case DEV_FM:
        switch (o->set.filter) { VOICE_FILTERS(DEV_FM) }
        break;
    default:
        switch (o->set.filter) { VOICE_FILTERS(DEV_PULSE) }
    }
}

/* The room's answer to `wet`, added to `sum`. */
static void room_run(SynthRoom *const r, const int32_t *const wet, int32_t *const sum, const int n)
{
    for (int i = 0; i < n; i++) {
        const int32_t in = wet[i] / 2;
        int32_t out = 0;

        for (int c = 0; c < SYNTH_COMBS; c++) {
            int32_t *const at = &r->comb[c][r->comb_at[c]];

            out += *at;
            r->damp[c] += (*at - r->damp[c]) / 2;
            *at = in + r->damp[c] * ROOM_FEEDBACK / 256;
            r->comb_at[c] = (uint16_t)(r->comb_at[c] + 1 == r->comb_len[c] ? 0 : r->comb_at[c] + 1);
        }
        for (int p = 0; p < SYNTH_PASSES; p++) {
            int32_t *const at = &r->pass[p][r->pass_at[p]];
            const int32_t back = *at;

            *at = out + back / 2;
            out = back - out;
            r->pass_at[p] = (uint16_t)(r->pass_at[p] + 1 == r->pass_len[p] ? 0 : r->pass_at[p] + 1);
        }
        sum[i] += out / 2;
    }
}

/* The voices into `sum`, and the room's answer to those it answers: each of them alone first, so its share can be
   sent. With none and the tail over, the room costs nothing. In stereo (`left` and `right` given) `sum` is the
   middle, in both; a voice to one side is in `left` and `right` instead, the far side's share less by its pan. */
static void voices(Synth *const s, int32_t *const sum, int32_t *const left, int32_t *const right, const int n)
{
    int32_t wet[SYNTH_BLOCK], one[SYNTH_BLOCK];
    bool sent = false;

    for (int v = 0; v < DEV_VOICES; v++) {
        SynthOsc *const o = &s->osc[v];
        const bool silent = o->level == 0 && (o->stage == STAGE_CUT || o->stage == STAGE_RELEASE);
        const int to = o->set.pan ? o->set.pan - PAN_MIDDLE : 0; /* -7 the left, 7 the right */
        const bool aside = left && to && !silent;
        const bool roomed = o->set.reverb && !silent;

        if (!aside && !roomed) {
            voice(s, o, sum, n);
            continue;
        }
        if (roomed && !sent && !s->room.live)
            s->room = (SynthRoom){.comb_len = {s->room.comb_len[0], s->room.comb_len[1], s->room.comb_len[2],
                                               s->room.comb_len[3]},
                                  .pass_len = {s->room.pass_len[0], s->room.pass_len[1]}};
        for (int i = 0; i < n; i++) {
            one[i] = 0;
            wet[i] = sent || !roomed ? wet[i] : 0;
        }
        voice(s, o, one, n);
        if (aside) {
            const int32_t near = PAN_MIDDLE - 1, far = near - (to < 0 ? -to : to);
            int32_t *const more = to < 0 ? left : right, *const less = to < 0 ? right : left;

            for (int i = 0; i < n; i++) {
                more[i] += one[i];
                less[i] += one[i] * far / near;
            }
        } else
            for (int i = 0; i < n; i++)
                sum[i] += one[i];
        if (roomed) {
            sent = true;
            for (int i = 0; i < n; i++)
                wet[i] += one[i] * o->set.reverb / 16;
        }
    }
    if (sent)
        s->room.live = s->rate * ROOM_TAIL;
    else if (s->room.live) {
        for (int i = 0; i < n; i++)
            wet[i] = 0;
        s->room.live -= s->room.live < (uint32_t)n ? s->room.live : (uint32_t)n;
    } else
        return;
    room_run(&s->room, wet, sum, n);
}

/* The mix at twice its sum, held under full: a block that would pass it is turned down, from its first sample, by as
   much as its loudest needs, and the gain climbs back at `release`. Many voices at once were clipped flat; kept under
   full by their volumes alone they were 8 dB under a recording. A block under full with nothing held is its sum
   doubled, no more.
   `sum` is `sides` channels, a sample of each in turn, turned down as one. */
static void limit(Synth *const s, const int32_t *const sum, int16_t *const out, const int frames, const int sides)
{
    const int n = frames * sides;
    int32_t top = 0;

    for (int i = 0; i < n; i++) {
        const int32_t size = sum[i] < 0 ? -sum[i] : sum[i];

        top = size > top ? size : top;
    }
    top *= 2;
    if (top <= LOUDEST && !s->held) {
        for (int i = 0; i < n; i++)
            out[i] = (int16_t)(sum[i] * 2);
        return;
    }

    const int32_t need = top > LOUDEST ? FULL - LOUDEST * (FULL / top) : 0;
    const int32_t eased = s->held > s->release * frames ? s->held - s->release * frames : 0;
    const int32_t held = need > eased ? need : eased;
    const int32_t from = need > s->held ? need : s->held; /* down at once: a ramp down let its first samples clip */
    const int32_t step = (held - from) / frames;

    for (int i = 0; i < n; i++) {
        const int32_t v = sum[i] * 2 * ((FULL - from - step * (i / sides + 1)) >> GAIN_SHIFT) >> GAIN_SHIFT;

        out[i] = (int16_t)(v > LOUDEST ? LOUDEST : v < -LOUDEST ? -LOUDEST : v);
    }
    s->held = held;
}

/* A block at a time, each voice over the block in turn: the sum is the same, an int32 till it is limited. With
   `sides` 2 a frame is its left sample then its right. */
static void render(Synth *const s, int16_t *const out, const int count, const int sides)
{
    for (int at = 0, n; at < count; at += n) {
        int32_t sum[SYNTH_BLOCK];

        n = count - at < SYNTH_BLOCK ? count - at : SYNTH_BLOCK;
        if (s->tick) { /* a block ends where the clock ticks */
            if (s->tick_left < (uint32_t)ONE) {
                s->tick();
                s->tick_left += s->tick_step;
            }
            n = (uint32_t)n < s->tick_left >> 16 ? n : (int)(s->tick_left >> 16);
            s->tick_left -= (uint32_t)n << 16;
        }

        if (!__atomic_load_n(&s->want, __ATOMIC_RELAXED) && !s->hertz && !s->gain) /* silent: speaker() is 0 */
            for (int i = 0; i < n; i++)
                sum[i] = 0;
        else
            for (int i = 0; i < n; i++)
                sum[i] = speaker(s);
        if (sides == 1) {
            voices(s, sum, nullptr, nullptr, n);
            limit(s, sum, out + at, n, 1);
        } else {
            int32_t left[SYNTH_BLOCK] = {0}, right[SYNTH_BLOCK] = {0}, both[SYNTH_BLOCK * 2];

            voices(s, sum, left, right, n);
            for (int i = 0; i < n; i++) {
                both[i * 2] = sum[i] + left[i];
                both[i * 2 + 1] = sum[i] + right[i];
            }
            limit(s, both, out + at * 2, n, 2);
        }
    }
}

void synth_render(Synth *const s, int16_t *const out, const int count)
{
    render(s, out, count, 1);
}

void synth_render_stereo(Synth *const s, int16_t *const out, const int frames)
{
    render(s, out, frames, 2);
}
