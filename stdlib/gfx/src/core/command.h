/* Commands: each draw call as it was drawn, after its target's camera, with the style's numbers;
 * the frame's log of them, which display.get_drawn reads: the same on every backend, so tests
 * assert on it.
 */
#ifndef GFX_COMMAND_H
#define GFX_COMMAND_H
#include "base/geom.h"
#include "core/style.h"

/* x, y the anchor or the first end; x2, y2 the other end. kind is display's DREW_*. */
typedef struct {
    int32_t kind;
    uint32_t target, source;
    float x, y, x2, y2, angle, scale;
    int32_t flags, colour, light;
    uint32_t tint;
    int32_t alpha, palette_row;
} Command;

/* The camera over one target: a world point p lands at m p + move, m the identity where shifted; a
   command turns by angle more and scales by zoom. clip: the target's. */
typedef struct {
    float m11, m12, m21, m22;
    Vec move;
    float angle, zoom;
    Rect clip;
    bool shifted;
} Place;

/* A frame's first CMD_KEPT calls, display.draws()'s most; calls counts them all. */
enum { CMD_KEPT = 32767 };

typedef struct {
    Command *items;
    uint32_t n, cap, calls;
} CmdLog;

/* The camera over a target w by h. */
Place cmd_where(const Camera *camera, int32_t w, int32_t h);

/* Where world point v lands: shifted, by two adds, not the multiplies. */
static inline Vec cmd_at(const Place *const p, const Vec v)
{
    if (p->shifted)
        return (Vec){v.x + p->move.x, v.y + p->move.y};

    return (Vec){
        p->m11 * v.x + p->m12 * v.y + p->move.x,
        p->m21 * v.x + p->m22 * v.y + p->move.y,
    };
}

/* The world's point that lands at v. */
static inline Vec cmd_back(const Place *const p, const Vec v)
{
    const float x = v.x - p->move.x, y = v.y - p->move.y;
    const float det = p->m11 * p->m22 - p->m12 * p->m21;
    return (Vec){(p->m22 * x - p->m12 * y) / det, (p->m11 * y - p->m21 * x) / det};
}

/* Neither turned nor zoomed: a shape stays upright and its size. */
static inline bool cmd_upright(const Place *const p) { return p->angle == 0 && p->zoom == 1; }

/* A draw call's command placed: its points by the camera, its angle and scale with it; the
   style's numbers. */
void cmd_place(Command *c, const Style *style, const Place *where);
/* c logged: false, no memory. */
bool cmd_log(CmdLog *log, const Command *c);
/* A frame begins. */
void cmd_clear(CmdLog *log);
void cmd_done(CmdLog *log);
#endif
