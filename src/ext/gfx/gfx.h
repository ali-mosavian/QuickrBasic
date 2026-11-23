/* gfx's state, and what every entry shares: errors raised, handles and the pen's target resolved.
 * The entries are entry_*.c, a part each.
 *
 * # How it works
 * One state, made from the host's memory when an entry first needs it and freed at the run's end
 * (gfx_stop): nothing is static but its pointer. An entry checks its arguments, raises an error
 * through the glue's qx_fail and returns: the calling line raises it once the entry returns.
 */
#ifndef GFX_GFX_H
#define GFX_GFX_H
#include "colour.h"
#include "command.h"
#include "gfx_qx.h"
#include "soft/soft.h"
#include "surface.h"

enum : uint8_t {
    ERR_ILLEGAL = 5,
    ERR_MEMORY = 7,
    ERR_SUBSCRIPT = 9,
    ERR_NOT_FOUND = 53,
    ERR_UNAVAILABLE = 73,
};

enum { GX_MOST = 1 << 14 }; /* pixels a side */

typedef struct {
    Surfaces surfaces;
    Palette palette;
    Pen pen;
    Camera camera;
    CmdLog log;  /* since the last flip */
    Vec *points; /* a call's points placed */
    uint32_t npoints;
    bool open; /* the screen */
    float hz;
} Gx;

/* The state: null, no memory or a host without a screen (raised). */
Gx *gx_get(void);
/* code raised: false. */
bool gx_fail(uint8_t code);
/* h's surface: null, none (raised). */
Surface *gx_surface(Gx *g, uint32_t h);
/* a's elements, elem bytes each, at least need of them: false, raised. */
bool gx_array(const QxArray *a, uint32_t elem, uint32_t need);
/* a's n elements of elem bytes from BASIC's index first: null, raised. */
const void *gx_elements(const QxArray *a, uint32_t elem, int32_t first, int32_t n);
/* Room for n points placed: null, raised. */
Vec *gx_points(Gx *g, uint32_t n);
/* A draw call begun: its flags (those of allowed, BLEND or ADD) and colour checked, the pen's
   target resolved into t and the camera over it into *where, c placed (cmd_place) and logged, the
   ink of its flags and the pen made: false, raised. */
bool gx_draw(Gx *g, Command *c, int32_t allowed, SoftTarget *t, Place *where);
#endif
