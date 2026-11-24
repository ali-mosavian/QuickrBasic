/* gfx's unit tests: calls on the session's host table, the state fresh for each test. */
#ifndef GFX_TEST_TEST_H
#define GFX_TEST_TEST_H
#include "call.h"
#include "core/surface.h"
#include "plat/pgfx.h"
#include "session.h"
#include "unit.h"

static inline const QxModule *gfx_fresh(Session *const S)
{
    const QxModule *const m = qx_init_gfx(&S->natv.host);
    m->stop();
    return m;
}

#define CALL(name, ...) CALL_ON(&S->natv.host, name __VA_OPT__(, ) __VA_ARGS__)
/* An index of the screen's surface. */
#define PX(x, y) CALL("surface.point", I(SURF_SCREEN), I(x), I(y)).result.i

/* A w by h screen at 8 bits, cleared to 1. */
#define CANVAS(w, h)                                                                               \
    do {                                                                                           \
        gfx_fresh(S);                                                                              \
        CHECK(CALL("display.open", I(w), I(h), I(8), I(0)).result.i == (int32_t)SURF_SCREEN);      \
        CHECK(!CALL("draw.clear", I(SURF_SCREEN), I(1)).error);                                    \
    } while (0)

/* The canvas's row y from x 0 as want's characters, '0' and the index. */
#define ROW(y, want)                                                                               \
    do {                                                                                           \
        char got_[64];                                                                             \
        const int32_t n_ = (int32_t)strlen(want);                                                  \
        for (int32_t x_ = 0; x_ < n_; x_++)                                                        \
            got_[x_] = (char)('0' + PX(x_, y));                                                    \
        CHECK_TEXT(got_, n_, want);                                                                \
    } while (0)
#endif
