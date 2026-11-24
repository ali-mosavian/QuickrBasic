/* Backends: a call's command drawn into its target. The state holds the screen's table (gfx.h), so
 * an entry draws through it and never asks which backend it is.
 *
 * # How it works
 * A backend keeps what it needs in its own state (Gx.own), made when it first needs it and freed
 * by stop. begin readies a call into its target; source readies it for a picture's texels,
 * solid for one colour over the picture's shape. The rest draw: points name pixel centres
 * (cover.h), and a picture's corners carry its texels (shape.h).
 */
#ifndef GFX_BACKEND_H
#define GFX_BACKEND_H
#include "core/command.h"
#include "core/surface.h"
#include "gfx_qx.h"

typedef struct Gx Gx;

/* poly's own flag, past the API's: u repeats every width. */
enum { POLY_WRAP = 1 << 30 };

/* What a GPU was asked since last counted: draws (batches), texture uploads, reads waited for. */
typedef struct {
    uint32_t batches, uploads, waits;
} BackendCount;

typedef struct {
    int32_t has; /* display.HAS_ */
    /* The screen the host gave, its surface of format: false, raised. */
    bool (*open)(Gx *g, const QxScreen *s, PxFormat f);
    /* The frame shown; the screen's pixels kept. */
    void (*flip)(Gx *g);
    /* Before the screen closes. */
    void (*close)(Gx *g);
    /* Its own state freed, nothing drawn or read back: another backend takes over, or the run
       ends. */
    void (*stop)(Gx *g);
    /* The count since the last call, zeroed. */
    BackendCount (*counted)(Gx *g);
    /* h's pixels current for the CPU: false, raised. */
    bool (*sync)(Gx *g, uint32_t h);
    /* Rows top to bottom of the root h written by the CPU. */
    void (*wrote)(Gx *g, uint32_t h, int32_t top, int32_t bottom);
    /* h, a surface, freed. */
    void (*forget)(Gx *g, uint32_t h);
    /* c's call readied into its target, whose picture is into, within clip: false, raised. */
    bool (*begin)(Gx *g, const Command *c, const Picture *into, Rect clip);
    /* The call readied for p's texels: false, raised. */
    bool (*source)(Gx *g, const Picture *p);
    /* Every texel drawn is colour, through the style as a shape's: SOLID's, with a colour of the
       call's. */
    void (*solid)(Gx *g, uint32_t colour);
    void (*clear)(Gx *g, uint32_t colour);
    void (*pset)(Gx *g, Vec p, uint32_t colour);
    /* Both ends drawn. */
    void (*line)(Gx *g, Vec a, Vec b, uint32_t colour);
    /* Closed, each pixel once. */
    void (*outline)(Gx *g, const Vec *p, uint32_t n, uint32_t colour);
    /* Convex, filled, n corners 3 up to POLY_MOST: p's texels, colour when p is null; its depth
       tested and set by flags into the target's; POLY_WRAP, u repeats. */
    void (*poly)(
        Gx *g,
        const Picture *p,
        const Corner *c,
        uint32_t n,
        uint32_t colour,
        int32_t flags
    );
    /* into's depth, all far: false, raised. */
    bool (*clear_depth)(Gx *g, const Picture *into);
    void (*circle)(Gx *g, Vec c, float r, uint32_t colour, bool fill);
    void (*arc)(Gx *g, Vec c, float r, float start, float finish, uint32_t colour);
    /* Upright, corners a and b drawn. */
    void (*rect)(Gx *g, Vec a, Vec b, uint32_t colour, bool fill);
    /* Upright at its size, its top left texel at at. */
    void (*sprite)(Gx *g, const Picture *p, Vec at, int32_t flags);
} Backend;

extern const Backend backend_soft, backend_gpu;
#endif
