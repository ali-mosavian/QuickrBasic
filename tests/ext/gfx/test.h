/* gfx's unit tests: calls on the session's host table, the state fresh for each test. */
#ifndef GFX_TEST_TEST_H
#define GFX_TEST_TEST_H
#include "call.h"
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
/* An index of the canvas. */
#define PX(x, y) CALL("surface.point", I(GFX_SURFACE_CANVAS), I(x), I(y)).result.i
#endif
