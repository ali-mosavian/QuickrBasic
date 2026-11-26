/* What 3dfx's drivers share: accel/driver.h's table over an SST card, each chip's own memory and
 * showing through a TdfxChip, each family's own bring-up through a TdfxFamily.
 *
 * # How it works
 * The frame buffer is colour buffers 0 and 1 and one of depth, a row each of the same stride. The
 * mode shown is the top rows of one colour buffer, drawn in the other, the screen centred in it; a
 * target is rows below the mode in buffer 0, drawn in it whether shown or not, its depth the same
 * rows of the depth buffer. A texture is one LOD of TMU0's memory. Registers are written as the
 * FIFO has room.
 */
#ifndef TDFX_TDFX_H
#define TDFX_TDFX_H
#include "plat/dos32/accel/driver.h"
#include "plat/dos32/drivers/3dfx/space.h"

/* How long a wait for the card lasts. */
enum : uint32_t { TDFX_WAIT_US = 100000 };

typedef struct Tdfx Tdfx;

typedef struct {
    uint32_t at; /* its texels' first byte in TMU0's memory */
    uint16_t w, h;
    uint32_t level, lod, base; /* setup.h's; tLOD's and texBaseAddr's words */
} TdfxTexture;

/* A chip's (SST-1's and CVG's, sst.c; H3's, voodoo3.c):
   - room: status' FIFO entries free;
   - based: texBaseAddr's word for texture memory's byte at;
   - upload: texels r of texture t from texels, pitch a row, TMU0 placing t;
   - drawn_in: the draws aimed at colour buffer `buffer`, fbzMode's draw buffer for it;
   - pixels: where buffer's row 0 is read, lfb_row bytes a row, once the card is idle;
   - show: buffer shown ^ 1 shown, at the retrace with vsync;
   - carry: the next frame made the shown one's. */
typedef struct {
    uint32_t room;
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
} TdfxChip;

/* A family's: its chip; its card (Tdfx's pci) readied, showing a mode holding w by h (Tdfx's to
   fill named there), false, it cannot; stopped, the VGA's screen shown. */
typedef struct {
    const char *name;
    const TdfxChip *chip;
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
    uint32_t lfb_row, stride, rows, mode_w, mode_h, texture_bytes, millihz;

    uint16_t w, h, left, top; /* the screen, and its place in the mode */
    uint32_t shown;           /* the colour buffer shown */
    uint32_t room;            /* FIFO entries known free */
    TdfxSpace memory, below;  /* texture memory's bytes; buffer 0's rows below the screen */
    TdfxTexture textures[ACCEL_TEXTURES];
    TdfxTarget targets[ACCEL_TARGETS];

    /* what the draws go to and how */
    uint32_t aimed, buffer, draw; /* a target slot; its colour buffer; fbzMode's draw buffer */
    uint32_t ox, oy;              /* its place in the buffer */
    AccelRect clip;
    AccelState state;
    uint32_t placed; /* the texture TMU0's registers place: ACCEL_TEXTURES, none */
};

/* A word to a window's at, as the FIFO has room. */
void tdfx_store(Tdfx *c, volatile uint8_t *at, uint32_t v);
void tdfx_put(Tdfx *c, uint32_t reg, uint32_t v);
/* An init register's (fbiInit, the video timing, clutData, dacData): not the FIFO's, written
   with the card idle. */
void tdfx_put_init(const Tdfx *c, uint32_t reg, uint32_t v);
void tdfx_put_float(Tdfx *c, uint32_t reg, float v);
uint32_t tdfx_get(const Tdfx *c, uint32_t reg);
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
