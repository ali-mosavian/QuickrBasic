/* VBE's graphics modes for gfx's screen on DOS (pgfx.c): what a mode is, which one shows a canvas,
 * and where. No call to the card here: the unit tests run it.
 */
#ifndef PLAT_DOS32_VBE_H
#define PLAT_DOS32_VBE_H
#include <stdint.h>

enum : uint16_t { VBE_MODE_13H = 0x13, VBE_V2 = 0x200 };
enum { VBE_INFO_BYTES = 256 };

/* A mode gfx shows in: a linear frame buffer at base, else written through a window of
   window_bytes at segment window_seg, set in steps of granule bytes. */
typedef struct {
    uint16_t number; /* 4F02h's; VBE_MODE_13H VGA's */
    uint16_t w, h;
    uint32_t pitch; /* bytes a row */
    uint8_t depth;  /* 8, 15 (x555), 16 (565) or 32 (x888) */
    bool linear;
    uint8_t window; /* 0 A, 1 B */
    uint16_t window_seg;
    uint32_t base, granule, window_bytes;
} VbeMode;

/* Where a w by h canvas shows in a mode: k times its size, from x, y. */
typedef struct {
    uint16_t k, x, y;
} VbeFit;

/* Mode `number`'s information (4F01h's block) from a card of VBE `version`: false, not one gfx
   shows in. */
bool vbe_mode(uint16_t number, uint16_t version, const uint8_t info[VBE_INFO_BYTES], VbeMode *m);
/* VGA's mode 13h. */
VbeMode vbe_mode_13h(void);
/* A depth's bytes a pixel: 0, none. */
uint8_t vbe_bytes(uint8_t depth);
/* Where a w by h canvas shows in m: k 0, it does not fit. */
VbeFit vbe_fit(const VbeMode *m, uint16_t w, uint16_t h);
/* Of n modes, the one that shows a w by h canvas of depth bits: of its size, else the least that
   holds it, its whole multiples first. -1, none. */
int32_t vbe_pick(const VbeMode *modes, uint32_t n, uint16_t w, uint16_t h, uint8_t depth);
/* The smallest of n modes of depth with a linear frame buffer holding a w by h canvas at its own
   size: its index; -1, none. */
int32_t vbe_holding(const VbeMode *modes, uint32_t n, uint16_t w, uint16_t h, uint8_t depth);
/* w pixels of `bytes` each from `from`, each k times across `to`. */
void vbe_widen(uint8_t *to, const uint8_t *from, uint32_t w, uint8_t bytes, uint16_t k);
/* Of n bytes at video memory offset `at`, the most one window position holds: *step the window's
   position (granules), *in the offset in the window. */
uint32_t vbe_window(const VbeMode *m, uint32_t at, uint32_t n, uint32_t *step, uint32_t *in);
#endif
