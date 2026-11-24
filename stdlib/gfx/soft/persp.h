/* The perspective span filler: qgl's b8span.asm PTEX and ATEX fillers (PDIV, PCLAMP, PSTEP,
 * PMKSTEP, ASTEP, AMKSTEP), format-generic, translated.
 *
 * # How it works
 * u/z, v/z and 1/z (scanner.h) are linear on the screen; u = (u/z) / (1/z) is taken every
 * SCAN_SUBDIVP pixels (PTEX), or at the span's two ends (ATEX), and a run between two is walked by
 * a constant 16.16 step. A divide's result rounds (fistp) and takes half a texel on its own side of the
 * divide. qgl's bound mode is off: a run's ends are not clamped, as the last run's end lies a
 * whole run on, past the span, and clamped would bend its step. Each texel is held inside the
 * picture instead, u repeating every width when wrap.
 *
 * Depth is qgl's PTEX_BODY's: a pixel tested against the depth there, then its depth written,
 * stepped by a constant 16.16 from the span's first; a keyed texel writes none. Left out: the
 * lightmaps. The steps are plain 16.16, not qgl's v shifted and masked for a power-of-two texture;
 * each pixel reads one texel, keyed unless opaque.
 */
#ifndef GFX_PERSP_H
#define GFX_PERSP_H
#include "soft/fill.h"
#include "soft/scanner.h"

/* d: the span's first pixel in its row, bytes each; s: the picture. ink: the texels' way into d,
   put a pixel at a time, s the target's format when null. depth: the span's first's, tested and
   set as asked; null, none. */
typedef struct {
    uint8_t *d;
    uint32_t bytes;
    Pixels s;
    bool opaque, wrap, whole;
    const Ink *ink;
    FillInk *put;
    uint16_t *depth;
    bool test, set;
} PerspDraw;

/* The span's pixels: whole, ATEX's one run. */
void persp_span(const PerspDraw *k, const ScanSpan *sp);
#endif
