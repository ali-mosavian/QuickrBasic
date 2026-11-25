/* Geometry: rectangles, points and corners, a coordinate a pixel's centre. */
#ifndef GFX_GEOM_H
#define GFX_GEOM_H
#include <stdint.h>

typedef struct {
    int32_t x, y, w, h;
} Rect;

/* A point; a coordinate names a pixel's centre. */
typedef struct {
    float x, y;
} Vec;

/* A polygon's corner on the target: z its distance, 1 or more; u, v its texel, in texels from the
   top left. */
typedef struct {
    float x, y, z, u, v;
} Corner;

/* A sprite's texels on the target, its turn: texel u, v, from the top left corner of its top left
   texel, lands at at + u * across + v * down. */
typedef struct {
    Vec at, across, down;
} Turn;

/* A surface's camera: the world's point at its top left; or, centred, the world's point at its
   centre, the camera turned by angle and the world zoomed. Zeroed, the world as it is. */
typedef struct {
    float x, y, angle, zoom;
    bool centred;
} Camera;

static inline int32_t rect_max(const int32_t a, const int32_t b) { return a > b ? a : b; }

static inline int32_t rect_min(const int32_t a, const int32_t b) { return a < b ? a : b; }

/* Where a and b overlap: w or h 0, nowhere. */
static inline Rect rect_meet(const Rect a, const Rect b)
{
    const int32_t x = rect_max(a.x, b.x), y = rect_max(a.y, b.y);
    const int32_t r = rect_min(a.x + a.w, b.x + b.w), d = rect_min(a.y + a.h, b.y + b.h);
    return (Rect){x, y, rect_max(r - x, 0), rect_max(d - y, 0)};
}
#endif
