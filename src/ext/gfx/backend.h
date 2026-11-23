/* Backends: a call's command drawn into its target. The state holds the screen's table (gfx.h), so
 * an entry draws through it and never asks which backend it is.
 *
 * # How it works
 * begin readies a call's target from the pen; source readies it for a picture's texels, solid for
 * one colour over the picture's shape. The rest draw: points name pixel centres (scan.h), and a
 * picture's corners carry its texels (shape.h).
 */
#ifndef GFX_BACKEND_H
#define GFX_BACKEND_H
#include "command.h"
#include "gpu/gpu.h"
#include "gfx_qx.h"
#include "soft/soft.h"
#include "surface.h"

typedef struct Gx Gx;

/* A call's target: the pen's clip in it, and what its backend readied. */
typedef struct {
    Rect clip;
    SoftTarget soft;
    GpuTarget gpu;
} Target;

typedef struct {
    /* The screen the host gave, its canvas of format: false, raised. */
    bool (*open)(Gx *g, const QxScreen *s, PxFormat f);
    /* The frame shown; the canvas's pixels kept. */
    void (*flip)(Gx *g);
    /* Before the screen closes. */
    void (*close)(Gx *g);
    /* h's pixels current for the CPU: false, raised. */
    bool (*sync)(Gx *g, uint32_t h);
    /* h, a surface, freed. */
    void (*forget)(Gx *g, uint32_t h);
    /* t readied for c's call into s, the pen's target: false, raised. */
    bool (*begin)(Gx *g, Surface *s, const Command *c, Target *t);
    /* t readied for p's texels: false, raised. */
    bool (*source)(Gx *g, Target *t, const Picture *p);
    /* Every texel drawn is colour, through the pen as a shape's: SOLID's, with a colour of the
       call's. */
    void (*solid)(const Gx *g, Target *t, uint32_t colour);
    void (*clear)(const Target *t, uint32_t colour);
    void (*pset)(const Target *t, Vec p, uint32_t colour);
    /* Both ends drawn. */
    void (*line)(const Target *t, Vec a, Vec b, uint32_t colour);
    /* Closed, each pixel once. */
    void (*outline)(const Target *t, const Vec *p, uint32_t n, uint32_t colour);
    /* Convex, filled. */
    void (*poly)(const Target *t, const Vec *p, uint32_t n, uint32_t colour);
    void (*circle)(const Target *t, Vec c, float r, uint32_t colour, bool fill);
    void (*arc)(const Target *t, Vec c, float r, float start, float finish, uint32_t colour);
    /* Upright, corners a and b drawn. */
    void (*rect)(const Target *t, Vec a, Vec b, uint32_t colour, bool fill);
    /* Upright at its size, its top left texel at at. */
    void (*sprite)(const Target *t, const Picture *p, Vec at, int32_t flags);
    /* n corners, 3 or 4 (soft.h's soft_texture). */
    void (*texture)(
        const Target *t,
        const Picture *p,
        const Vertex *v,
        uint32_t n,
        int32_t flags,
        bool wrap
    );
} Backend;

extern const Backend backend_soft, backend_gpu;
#endif
