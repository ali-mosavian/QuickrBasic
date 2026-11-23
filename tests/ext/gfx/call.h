/* gfx's tests call it as BASIC does: its entries by name, through the glue, on the host's table
   (io/native.c), the state fresh for each test. */
#ifndef GFX_TEST_CALL_H
#define GFX_TEST_CALL_H
#include "gfx_qx.h"
#include "io/native.h"
#include "plat/pgfx.h"
#include "session.h"
#include "unit.h"

/* A call's error (0 none) and result. */
typedef struct {
    uint8_t error;
    QxArg result;
} Called;

static inline const QxModule *gfx_fresh(Session *const S)
{
    const QxModule *const m = qx_init_gfx(&S->natv.host);
    m->stop();
    return m;
}

static inline Called gfx_call(Session *const S, const char *const name, const QxArg *const args)
{
    const QxModule *const m = qx_init_gfx(&S->natv.host);
    const QxEntry *const e = natv_entry(m, name, (uint32_t)strlen(name));
    QxCall c = {.host = &S->natv.host, .arg = args};

    if (!e)
        return (Called){.error = 255};

    e->fn(&c);
    return (Called){c.error, c.result};
}

#define I(v) ((QxArg){.i = (int32_t)(v)})
#define F(v) ((QxArg){.f = (float)(v)})
#define STR(text) ((QxArg){.s = {text, sizeof text - 1}})
#define ARRAY(array) ((QxArg){.a = (array)})
#define REF(p) ((QxArg){.ref = (p)})
#define CALL(name, ...) gfx_call(S, name, (const QxArg[]){__VA_ARGS__ __VA_OPT__(, )(QxArg){}})
/* An index of the canvas. */
#define PX(x, y) CALL("surface.point", I(GFX_SURFACE_CANVAS), I(x), I(y)).result.i

/* A LONG or BYTE array of n elements over data. */
static inline QxArray gfx_array(void *const data, const uint32_t n, const uint32_t elem)
{
    return (QxArray){.data = data, .count = n, .elem = elem};
}
#endif
