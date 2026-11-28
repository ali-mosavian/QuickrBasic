/* gfx called as BASIC calls it: its entries by name, through the glue, on a host's table
   (io/native.c). The unit tests' and the Mac harness's. */
#ifndef GFX_TEST_CALL_H
#define GFX_TEST_CALL_H
#include <string.h>
#include "gfx_qx.h"
#include "io/native.h"

/* A call's error (0 none) and result. */
typedef struct {
    uint8_t error;
    QxArg result;
} Called;

static inline Called gfx_call(
    const QxHost *const host,
    const char *const name,
    const QxArg *const args
)
{
    const QxModule *const m = qx_init_gfx(host);
    const QxEntry *const e = natv_entry(m, "", name, (uint32_t)strlen(name));
    char why[FAULT_WHY];
    NatvCall c = {.c = {.host = host, .arg = args}, .why = why}; /* as vm/ncall.c makes it */

    if (!e)
        return (Called){.error = 255};

    e->fn(&c.c);
    return (Called){c.c.error, c.c.result};
}

/* A LONG or BYTE array of n elements over data. */
static inline QxArray gfx_array(void *const data, const uint32_t n, const uint32_t elem)
{
    return (QxArray){.data = data, .count = n, .elem = elem};
}

#define I(v) ((QxArg){.i = (int32_t)(v)})
#define F(v) ((QxArg){.f = (float)(v)})
#define STR(text) ((QxArg){.s = {text, sizeof text - 1}})
#define ARRAY(array) ((QxArg){.a = (array)})
#define REF(p) ((QxArg){.ref = (p)})
#define CALL_ON(host, name, ...)                                                                   \
    gfx_call(host, name, (const QxArg[]){__VA_ARGS__ __VA_OPT__(, )(QxArg){}})
#endif
