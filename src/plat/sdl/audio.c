/* The speaker: a square wave of the program's last tone, made in SDL's audio callback. A tone that
 * starts or stops ramps over a few milliseconds so it does not click; with no audio device the
 * program runs silent. The runtime's tones come here by wrap.h, and go on to its own speaker. */
#include "sdl.h"

enum { RATE = 44100, FADE = RATE / 500, LOUD = 4000, CHUNK = 1024 };

static uint32_t wanted; /* the tone set, Hz; 0 none */

void __real_dev_tone(unsigned hertz);

void __wrap_dev_tone(const unsigned hertz)
{
    __atomic_store_n(&wanted, hertz, __ATOMIC_RELAXED);
    __real_dev_tone(hertz);
}

static void fill(void *const user, SDL_AudioStream *const stream, const int more, const int total)
{
    static uint32_t hertz, phase;
    static int32_t gain;
    int16_t buf[CHUNK];

    for (int n = more / (int)sizeof buf[0]; n > 0;) {
        const int chunk = n < CHUNK ? n : CHUNK;

        for (int i = 0; i < chunk; i++) {
            const uint32_t want = __atomic_load_n(&wanted, __ATOMIC_RELAXED);

            if (want != hertz && gain == 0) { /* a new tone starts from silence */
                hertz = want;
                phase = 0;
            }

            gain += want == hertz && hertz ? gain < FADE : -(gain > 0);
            phase = (phase + hertz) % RATE;
            buf[i] = (int16_t)((phase < RATE / 2 ? LOUD : -LOUD) * gain / FADE);
        }

        SDL_PutAudioStreamData(stream, buf, chunk * (int)sizeof buf[0]);
        n -= chunk;
    }
}

void sdl_audio_open(void)
{
    const SDL_AudioSpec spec = {.format = SDL_AUDIO_S16, .channels = 1, .freq = RATE};

    SDL_AudioStream *const s =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, fill, nullptr);

    if (s)
        SDL_ResumeAudioStreamDevice(s);
}
