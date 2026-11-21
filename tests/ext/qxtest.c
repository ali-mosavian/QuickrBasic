/* qxtest: the native ABI's test extension, plain C functions glued to qxtest.bas. */
#include "qxtest_qx.h"

static char shouted[256];
static int32_t started;

void qxtest_init(void) {}

void qxtest_start(void) { started++; }

void qxtest_stop(void) {}

int32_t add(const int32_t a, const int32_t b) { return a + b; }

double half(const double d) { return d / 2; }

QxStr shout_text(const QxStr s)
{
    const uint32_t n = s.len < sizeof shouted ? s.len : sizeof shouted;

    for (uint32_t i = 0; i < n; i++)
        shouted[i] = s.ptr[i] >= 'a' && s.ptr[i] <= 'z' ? (char)(s.ptr[i] - 32) : s.ptr[i];

    return (QxStr){shouted, n};
}

void bump(int32_t *const n) { ++*n; }

void move(qxtest_pt *const p, const int32_t dx) { p->x = (int16_t)(p->x + dx); }

float total(const QxArray *const a)
{
    const float *const f = a->data;
    float sum = 0;

    for (uint32_t i = 0; i < a->count; i++)
        sum += f[i];

    return sum;
}

void fail(const int32_t code) { qx_fail((uint8_t)code); }

int32_t runs(void) { return started; }
