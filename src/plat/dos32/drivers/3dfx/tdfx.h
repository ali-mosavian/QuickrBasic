/* What 3dfx's drivers share: accel/driver.h's table over an SST card, each chip's own memory and
 * showing through a TdfxChip, each family's own bring-up through a TdfxFamily.
 *
 * # How it works
 * The frame buffer is colour buffers 0 and 1 and one of depth, a row each of the same stride. The
 * mode shown is the top rows of one colour buffer, drawn in the other, the screen centred in it; a
 * target is rows below the mode in buffer 0, drawn in it whether shown or not, its depth the same
 * rows of the depth buffer. A texture is one LOD of TMU0's memory; an indexed one's texels are
 * looked up in TMU0's palette, index 0 keyed by the chroma key: the palette's colour for it, its
 * low bits, which a 565 pixel drops, made unlike every other's. Registers are written as the chip
 * takes them, those a write picks: a triangle as the words its state reads, or as its corners
 * to a setup unit. Worked out here, a run of triangles of one colour sends it once, as color0's.
 * Where the CPU can, the window written most is write-combined, as Glide has it, and fenced
 * where the card must see writes whole and in order.
 */
#ifndef TDFX_TDFX_H
#define TDFX_TDFX_H
#include "plat/dos32/accel/driver.h"
#include "plat/dos32/drivers/3dfx/setup.h"
#include "plat/dos32/drivers/3dfx/space.h"

/* How long a wait for the card lasts. */
enum : uint32_t { TDFX_WAIT_US = 100000 };

/* 3dfx's PCI vendor ID. */
enum { TDFX_VENDOR = 0x121A };

typedef struct Tdfx Tdfx;

/* The registers a put writes, its first's on: a bit each in mask, and their places in order. */
typedef struct {
    uint32_t mask, n;
    uint8_t at[32];
} TdfxPicks;

/* How a draw's triangles are sent: the parameters read; fbzColorPath's word; their words, worked out
   on the CPU, or by the setup unit: sSetupMode's, a corner's words, and those written register by
   register, with the command beginning a triangle or drawing one. */
typedef struct {
    uint32_t params, path, mode;
    TdfxPicks sent, corner, begins, draws;
} TdfxWay;

typedef struct {
    uint32_t at; /* its texels' first byte in TMU0's memory */
    uint16_t w, h;
    bool indexed;              /* a byte a texel, P_8: else two, ARGB1555 */
    uint32_t level, lod, base; /* setup.h's; tLOD's and texBaseAddr's words */
} TdfxTexture;

/* A chip's (SST-1's and CVG's, sst.c; H3's, voodoo3.c):
   - busy: status' bits while it works;
   - put: the registers p picks from reg written, v's word at each one's place;
   - based: texBaseAddr's word for texture memory's byte at;
   - upload: texels r of texture t from texels, pitch a row, TMU0 placing t and taking its
     format;
   - drawn_in: the draws aimed at colour buffer `buffer`, fbzMode's draw buffer for it;
   - pixels: where buffer's row 0 is read, lfb_row bytes a row, once the card is idle;
   - show: buffer shown ^ 1 shown, at the retrace with vsync;
   - carry: the next frame made the shown one's;
   - paletted: after TMU0's palette is written, what holds texels looked up in it flushed;
   - cornered: n corners to the setup unit, three a triangle or one fan, each the words Tdfx's way's corner
     picks: null, written to its registers. */
typedef struct {
    uint32_t busy;
    void (*put)(Tdfx *c, uint32_t reg, const TdfxPicks *p, const uint32_t *v);
    uint32_t (*based)(const Tdfx *c, uint32_t at);
    void (*upload)(
        Tdfx *c,
        const TdfxTexture *t,
        AccelRect r,
        const uint16_t *texels,
        uint32_t pitch
    );
    uint32_t (*drawn_in)(Tdfx *c, uint32_t buffer);
    volatile uint8_t *(*pixels)(Tdfx *c, uint32_t buffer);
    void (*show)(Tdfx *c, bool vsync);
    void (*carry)(Tdfx *c);
    void (*paletted)(Tdfx *c);
    void (*cornered)(Tdfx *c, const TdfxVertex *v, uint32_t n, bool fan);
} TdfxChip;

/* A family's: its chip; whether it has the setup unit; its card (Tdfx's pci) readied, showing a
   mode holding w by h (Tdfx's to fill named there), false, it cannot; stopped, the VGA's screen
   shown. */
typedef struct {
    const char *name;
    const TdfxChip *chip;
    bool unit;
    bool (*open)(Tdfx *c, uint16_t w, uint16_t h);
    void (*close)(Tdfx *c);
} TdfxFamily;

typedef struct {
    uint32_t y; /* buffer 0's row: 0, none */
    uint16_t w, h;
    bool depth;
} TdfxTarget;

struct Tdfx {
    const AccelHost *host;
    const TdfxChip *chip;
    PciFunction pci;
    /* the family's open fills: the registers, the frame buffer's and textures' windows; the frame
       buffer window's bytes a row; a buffer's stride in pixels and rows; the mode shown's size;
       texture memory's bytes; the refresh */
    volatile uint8_t *regs, *lfb, *texels;
    /* the status tdfx_idle reads: H3's through its 2D registers, as 86Box answers it only once
       its render threads have drawn what they were queued; its 3D ones read idle before */
    const volatile uint32_t *status;
    uint32_t lfb_row, stride, rows, mode_w, mode_h, texture_bytes, millihz;
    void(ACCEL_CALL *fence)(void); /* the host's, a window write-combined: null, none is */
    volatile uint8_t *packer; /* the SST-1's: the word its PCI packer is flushed by, null none */

    uint16_t w, h, left, top; /* the screen, and its place in the mode */
    uint32_t shown;           /* the colour buffer shown */
    bool blits;               /* the family's open finds: a carry is the card's blit (sst.c) */
    TdfxSpace memory, below;  /* texture memory's bytes; buffer 0's rows below the screen */
    TdfxTexture textures[ACCEL_TEXTURES];
    TdfxTarget targets[ACCEL_TARGETS];

    /* what the draws go to and how */
    uint32_t aimed, buffer, draw; /* a target slot; its colour buffer; fbzMode's draw buffer */
    uint32_t ox, oy;              /* its place in the buffer */
    AccelRect clip;
    AccelState state;
    uint32_t params, path; /* setup.h's parameters the state reads; fbzColorPath's word */
    /* its ways: by the corners' colours, or of one colour, color0's, its parameters not sent, once
       worked out, coloured; the way the registers are set for; color0's word */
    TdfxWay ways[2];
    bool coloured;
    const TdfxWay *way;
    uint32_t colour0;
    uint32_t placed; /* the texture TMU0's registers place: ACCEL_TEXTURES, none */
};

/* mask's picks. */
TdfxPicks tdfx_picks(uint32_t mask);
void tdfx_put(Tdfx *c, uint32_t reg, uint32_t v);
/* n registers from reg written v's words in order, n under 32. */
void tdfx_put_run(Tdfx *c, uint32_t reg, const uint32_t *v, uint32_t n);
/* An init register's (fbiInit, the video timing, clutData, dacData): not the FIFO's, written
   with the card idle. */
void tdfx_put_init(const Tdfx *c, uint32_t reg, uint32_t v);
uint32_t tdfx_get(const Tdfx *c, uint32_t reg);
/* phys up to bytes, the window written most, write-combined if the CPU can, until uncombined:
   false, not. The family's open asks once the card is brought up, as init writes go one at a
   time. */
bool tdfx_combined(Tdfx *c, uint32_t phys, uint32_t bytes);
void tdfx_uncombined(Tdfx *c);

/* The writes combined so far sent on, in order: nothing, none combined. */
static inline void tdfx_fenced(const Tdfx *const c)
{
    if (c->fence)
        c->fence();
}

/* The card idle: its FIFO empty and nothing drawing. */
void tdfx_idle(Tdfx *c);
/* The FBI idle, nothing written to the FIFO: as the PCI FIFO may take no writes. */
void tdfx_idle_fbi(const Tdfx *c);
/* Pixels r of colour buffer `buffer` fast filled, as what has them: their colour argb, their depth
   0. */
void tdfx_cleared(Tdfx *c, uint32_t buffer, AccelRect r, uint32_t argb, uint8_t what);
/* clutData's entries: the video output looks a channel up as its 5 or 6 bits with zeros below,
   between two entries. */
enum { TDFX_CLUT = 33 };
/* clutData's entry i: a channel shown as gfx widens it, its top bits repeated below. */
uint32_t tdfx_clut(uint32_t i);
/* The gamma table written: the video running. */
void tdfx_gamma(Tdfx *c);
/* The driver's table, its family's: null, the host's ABI another. */
const AccelDriver *tdfx_driver(const AccelHost *host, const TdfxFamily *family);
#endif
