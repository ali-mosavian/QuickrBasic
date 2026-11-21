/* The host's audio stream, portable: the renders open and the mixer the device calls.
 *
 * # How it works
 * The renders are a short table, changed under the device's lock, so paud_mix, run where the
 * device plays, reads it whole. One render writes out itself; more each render a chunk into
 * scratch, added to out with clipping. Scratch is static: a Sound Blaster's interrupt runs on
 * whatever stack it finds, and one device mixes at a time.
 */
#include "plat/paud.h"
#include <string.h>

enum { STREAMS = 8, CHUNK = 256 };

static const PaudDevice *dev;
static PaudRender *streams[STREAMS];
static uint32_t nstreams;
static bool started;

void paud_device(const PaudDevice *const d) { dev = d; }

uint32_t paud_lock(void) { return dev ? dev->lock() : 0; }

void paud_unlock(const uint32_t token)
{
    if (dev)
        dev->unlock(token);
}

static bool find(PaudRender *const r, uint32_t *const at)
{
    for (uint32_t i = 0; i < nstreams; i++)
        if (streams[i] == r) {
            *at = i;
            return true;
        }

    return false;
}

void paud_close(PaudRender *const r)
{
    uint32_t i;

    if (!find(r, &i))
        return;

    const uint32_t token = paud_lock();
    streams[i] = streams[--nstreams];
    paud_unlock(token);

    if (!nstreams && started) {
        started = false;
        dev->stop();
    }
}

bool paud_open(PaudRender *const r, uint32_t *const rate, bool *const stereo)
{
    uint32_t i;

    if (!dev || (!find(r, &i) && nstreams == STREAMS))
        return false;
    if (!find(r, &i)) { /* in first: a device starting plays it at once */
        const uint32_t token = paud_lock();
        streams[nstreams++] = r;
        paud_unlock(token);
    }

    if (!started && !(started = dev->start())) {
        paud_close(r);
        return false;
    }

    *rate = dev->rate;
    *stereo = dev->stereo;
    return true;
}

void paud_end(void)
{
    while (nstreams)
        paud_close(streams[0]);
}

void paud_mix(int16_t *const out, const int32_t frames)
{
    static int16_t scratch[2 * CHUNK];
    const int32_t per = dev && dev->stereo ? 2 : 1;

    if (nstreams == 1) {
        streams[0](out, frames);
        return;
    }

    memset(out, 0, (size_t)(frames * per) * sizeof *out);

    for (uint32_t s = 0; s < nstreams; s++)
        for (int32_t at = 0; at < frames; at += CHUNK) {
            const int32_t n = frames - at < CHUNK ? frames - at : CHUNK;
            int16_t *const o = out + at * per;
            streams[s](scratch, n);

            for (int32_t i = 0; i < n * per; i++) {
                const int32_t sum = o[i] + scratch[i];
                o[i] = (int16_t)(sum > INT16_MAX ? INT16_MAX : sum < INT16_MIN ? INT16_MIN : sum);
            }
        }
}
