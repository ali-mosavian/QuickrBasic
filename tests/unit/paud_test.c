/* plat/audio/paud.c: the device started by the first stream and stopped by the last, one stream
   played as it renders, two summed and clipped, the lock nested by its tokens. */
#include "plat/paud.h"
#include "unit.h"
#include <string.h>

static uint32_t starts, stops, locks, unlocks;
static bool fails; /* the device will not start */

static bool start(void)
{
    starts++;
    return !fails;
}
static void stop(void) { stops++; }
static uint32_t lock(void) { return ++locks; }
static void unlock(const uint32_t token) { unlocks += token; }

static const PaudDevice device = {
    .rate = 22050,
    .stereo = true,
    .start = start,
    .stop = stop,
    .lock = lock,
    .unlock = unlock,
};

static void QX_CALL loud(int16_t *const out, const int32_t frames)
{
    for (int32_t i = 0; i < 2 * frames; i++)
        out[i] = 30000;
}

static void QX_CALL ramp(int16_t *const out, const int32_t frames)
{
    for (int32_t i = 0; i < 2 * frames; i++)
        out[i] = (int16_t)(8 * i);
}

static void reset(void)
{
    paud_end();
    paud_device(&device);
    starts = stops = locks = unlocks = 0;
    fails = false;
}

TEST(paud_starts_with_the_first_stream_stops_with_the_last)
{
    (void)S;
    reset();
    uint32_t rate;
    bool stereo;
    CHECK(paud_open(ramp, &rate, &stereo) && rate == 22050 && stereo && starts == 1);
    CHECK(paud_open(loud, &rate, &stereo) && starts == 1);
    CHECK(paud_open(loud, &rate, &stereo) && starts == 1); /* open already: as it was */
    paud_close(ramp);
    CHECK(!stops);
    paud_close(loud);
    CHECK(stops == 1);
    CHECK(paud_open(ramp, &rate, &stereo) && starts == 2);
    paud_end();
    CHECK(stops == 2);
}

TEST(paud_mixes_each_stream_clipped)
{
    (void)S;
    reset();
    uint32_t rate;
    bool stereo;
    int16_t out[600];
    CHECK(paud_open(ramp, &rate, &stereo));
    paud_mix(out, 300);
    CHECK(out[0] == 0 && out[599] == 8 * 599); /* one stream: as it renders, past a chunk */
    CHECK(paud_open(loud, &rate, &stereo));
    paud_mix(out, 300);
    CHECK(out[1] == 30008 && out[511] == INT16_MAX && out[512] == 30000); /* a chunk at a time */
    paud_end();
}

TEST(paud_refuses_with_no_device_or_none_started)
{
    (void)S;
    reset();
    uint32_t rate;
    bool stereo;
    fails = true;
    CHECK(!paud_open(ramp, &rate, &stereo));
    fails = false;
    int16_t out[4] = {1, 2, 3, 4};

    paud_mix(out, 2);
    CHECK(!out[0] && !out[3]); /* the render it refused not played */
    paud_device(nullptr);
    CHECK(!paud_open(ramp, &rate, &stereo));
    paud_device(&device);
}

TEST(paud_lock_passes_the_device_s_token)
{
    (void)S;
    reset();
    const uint32_t a = paud_lock(), b = paud_lock();
    paud_unlock(b);
    paud_unlock(a);
    CHECK(locks == 2 && unlocks == a + b);
    paud_device(nullptr);
    paud_unlock(paud_lock()); /* no device: nothing held */
    paud_device(&device);
}
