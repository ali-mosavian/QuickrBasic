/* gfx's state, and what every entry shares: errors raised, handles resolved, a draw call begun.
 * The entries are in api/, a part each.
 *
 * # How it works
 * One state, made from the host's memory when an entry first needs it and freed at the run's end
 * (gfx_stop): nothing is static but its pointer. An entry checks its arguments, raises an error
 * through the glue's qx_fail and returns: the calling line raises it once the entry returns.
 */
#ifndef GFX_GFX_H
#define GFX_GFX_H
#include "core/backend.h"
#include "core/command.h"
#include "core/font.h"
#include "core/palette.h"
#include "core/surface.h"
#include "gfx_qx.h"

enum : uint8_t {
    ERR_ILLEGAL = 5,
    ERR_MEMORY = 7,
    ERR_SUBSCRIPT = 9,
    ERR_NOT_FOUND = 53,
    ERR_UNAVAILABLE = 73,
};

enum { GX_BLENDS = GFX_DRAW_BLEND | GFX_DRAW_ADD };

typedef struct Gx {
    const Backend *backend; /* the screen's */
    void *own;              /* the backend's state: null till it makes it */
    Surfaces surfaces;
    Fonts fonts;
    Palette palette;
    Style style;
    CmdLog log;                  /* since the last flip */
    gfx_display_framestats cost; /* the frame shown last's */
    Vec *points;                 /* a call's points placed */
    uint32_t npoints;
    bool open; /* the screen */
    float hz;
} Gx;

/* The state: null, no memory or a host without a screen (raised). */
Gx *gx_get(void);
/* code raised: false. */
bool gx_fail(uint8_t code);
/* An illegal function call raised when bad: bad. */
bool gx_illegal(bool bad);
/* A size w by h a surface or a screen may have. */
bool gx_size(int32_t w, int32_t h);
/* b draws from now on: the last backend stopped. */
void gx_use(Gx *g, const Backend *b);
/* h's surface: null, none (raised). */
Surface *gx_surface(Gx *g, uint32_t h);
/* a's elements, elem bytes each, at least need of them: false, raised. */
bool gx_array(const QxArray *a, uint32_t elem, uint32_t need);
/* a's n elements of elem bytes from BASIC's index first: null, raised. */
const void *gx_elements(const QxArray *a, uint32_t elem, int32_t first, int32_t n);
/* Room for n points placed: null, raised. */
Vec *gx_points(Gx *g, uint32_t n);
/* r of h's pixels written by the CPU: counted, and told the backend. */
void gx_wrote(Gx *g, uint32_t h, Rect r);
/* dst and src, surfaces, are not over the same pixels (one surface, or its views): false, raised.
   A GPU cannot draw a texture into itself. */
bool gx_apart(Gx *g, uint32_t dst, uint32_t src);
/* A draw call into dst checked, nothing drawn or logged: dst, and c's source (0 none) apart from
   it; its flags (those of allowed, BLEND or ADD) and colour; dst's camera and clip into *where:
   false, raised. */
bool gx_check(Gx *g, uint32_t dst, const Command *c, int32_t allowed, Place *where);
/* A checked call begun: c placed into dst (cmd_place) and logged, the backend readied: false,
   raised. */
bool gx_begin(Gx *g, uint32_t dst, Command *c, const Place *where);
/* gx_check, then gx_begin. */
bool gx_draw(Gx *g, uint32_t dst, Command *c, int32_t allowed, Place *where);
#endif
