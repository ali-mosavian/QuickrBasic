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
};

enum Stage { STAGE_ATTACK, STAGE_DECAY, STAGE_RELEASE, STAGE_CUT };

/* SID's times from silence to full (attack) and from full to silence (decay, release), ms */
static const uint16_t attack_ms[16] = {2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000};
static const uint16_t decay_ms[16] = {6, 24, 48, 72, 114, 168, 204, 240, 300, 750, 1500, 2400, 3000, 9000, 15000, 24000};
/* SID's exponential fall: below each level (of 255) the step is divided by its divisor */
static const int32_t below[SYNTH_CURVE] = {256, 93, 54, 26, 14, 6};
static const int32_t divisor[SYNTH_CURVE] = {1, 2, 4, 8, 16, 30};

void synth_init(Synth *const s, const uint32_t rate)
{
    *s = (Synth){.rate = rate, .per_hertz = UINT32_MAX / rate, .rest = (UINT32_MAX % rate + 1) % rate};
    for (int i = 0; i < DEV_VOICES; i++) {
        s->osc[i].noise = 0x7FFFF8;
        s->osc[i].stage = STAGE_CUT;
    }
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

void synth_voice(Synth *const s, const unsigned voice, const DevVoice *const v)
{
    if (voice >= DEV_VOICES)
        return;
    SynthOsc *const o = &s->osc[voice];
    const bool attack = v->gate && (!o->set.gate || v->trigger != o->set.trigger);

    o->set = *v;
    o->step = v->hertz * s->per_hertz + v->hertz * s->rest / s->rate; /* hertz * 2^32 / rate */
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

/* The wave's next sample, -32767 to 32767. */
[[gnu::always_inline]] static inline int32_t wave(SynthOsc *const o)
{
    const uint32_t was = o->phase;

    o->phase += o->step;
    switch (o->set.wave) {
    case DEV_TRIANGLE: {
        const int32_t x = (int32_t)(o->phase >> 16);
        return (x < 32768 ? x * 2 : (65535 - x) * 2) - 32767;
    }
    case DEV_SAW:
        return (int32_t)(o->phase >> 16) - 32768;
    case DEV_NOISE:
        /* the register steps 16 times a period, as SID's on bit 19 of its 24 */
        if (~was & o->phase & 1u << 27) {
            const uint32_t bit = (o->noise >> 22 ^ o->noise >> 17) & 1;
            o->noise = (o->noise << 1 | bit) & 0x7FFFFF;
        }
        return (int32_t)(o->noise >> 15 & 0xFF) * 257 - 32767;
    default:
        return o->phase >> 20 < o->set.width ? 32767 : -32767;
    }
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
        for (const int end = i + run;;) {
            /* under 2^15 times 2^15, and unfiltered under 2^15 times the gain: int32 holds them */
            const int32_t out = wave(&o) * (o.level >> 9) >> 15;
            if (filter != DEV_UNFILTERED)
                sum[i] += (int32_t)((int64_t)filtered(&o, out) * o.gain >> 15);
            else
                sum[i] += out * o.gain >> 15;
            if (++i == end)
                break;
            o.level += delta;
        }
    }
    into->phase = o.phase;
    into->noise = o.noise;
    into->level = o.level;
    into->stage = o.stage;
    into->low = o.low;
    into->band = o.band;
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
    default:
        switch (o->set.filter) { VOICE_FILTERS(DEV_PULSE) }
    }
}

/* A block at a time, each voice over the block in turn: the sum is the same, an int32 till it is clipped. */
void synth_render(Synth *const s, int16_t *const out, const int count)
{
    for (int at = 0; at < count; at += SYNTH_BLOCK) {
        const int n = count - at < SYNTH_BLOCK ? count - at : SYNTH_BLOCK;
        int32_t sum[SYNTH_BLOCK];

        if (!__atomic_load_n(&s->want, __ATOMIC_RELAXED) && !s->hertz && !s->gain) /* silent: speaker() is 0 */
            for (int i = 0; i < n; i++)
                sum[i] = 0;
        else
            for (int i = 0; i < n; i++)
                sum[i] = speaker(s);
        for (int v = 0; v < DEV_VOICES; v++)
            voice(s, &s->osc[v], sum, n);
        for (int i = 0; i < n; i++)
            out[at + i] = (int16_t)(sum[i] > 32767 ? 32767 : sum[i] < -32767 ? -32767 : sum[i]);
    }
}
