/* What 3dfx's drivers share: accel/driver.h's table over an SST card, each family's own setting up
 * and showing of the screen through a TdfxFamily.
 *
 * # How it works
 * The frame buffer is two colour buffers and one of depth, a row each of the same stride. The mode
 * shown is the top rows of the two colour buffers, drawn in the one not shown, the screen centred in
 * it; a target is rows below the mode in buffer 0, drawn in it whether shown or not, its depth the
 * same rows of the depth buffer. A texture is one LOD of TMU0's memory. Registers are written as the
 * FIFO has room.
 */
#ifndef TDFX_TDFX_H
#define TDFX_TDFX_H
#include "plat/dos32/accel/driver.h"
#include "plat/dos32/drivers/3dfx/space.h"

typedef struct Tdfx Tdfx;

/* A family's: its card readied, showing a mode holding w by h (Tdfx's to fill named there), false,
   it cannot;
   stopped, the VGA's screen shown; the next frame made the shown one's. */
typedef struct {
    const char *name;
    bool (*open)(Tdfx *c, const PciFunction *card, uint16_t w, uint16_t h);
    void (*close)(Tdfx *c);
    void (*carry)(Tdfx *c);
} TdfxFamily;

typedef struct {
    uint32_t at; /* its texels' first byte in TMU0's memory */
    uint16_t w, h;
    uint32_t level, lod, base; /* setup.h's; tLOD's and texBaseAddr's words */
} TdfxTexture;

typedef struct {
    uint32_t y; /* buffer 0's row: 0, none */
    uint16_t w, h;
    bool depth;
} TdfxTarget;

struct Tdfx {
    const AccelHost *host;
    PciFunction pci;
    /* the family's open fills: the registers, the frame buffer's and textures' windows; a
       buffer's stride in pixels and rows; the mode shown's size; texture memory's bytes; the
       refresh */
    volatile uint8_t *regs, *lfb, *texels;
    uint32_t stride, rows, mode_w, mode_h, texture_bytes, millihz;

    uint16_t w, h, left, top; /* the screen, and its place in the mode */
    uint32_t shown; /* the colour buffer shown */
    uint32_t room;  /* FIFO entries known free */
    TdfxSpace memory, below; /* texture memory's bytes; buffer 0's rows below the screen */
    TdfxTexture textures[ACCEL_TEXTURES];
    TdfxTarget targets[ACCEL_TARGETS];

    /* what the draws go to and how */
    uint32_t aimed, draw; /* a target slot; its buffer, 0 front or 1 back */
    uint32_t ox, oy;      /* its place in the buffer */
    AccelRect clip;
    AccelState state;
    uint32_t placed; /* the texture TMU0's registers place: ACCEL_TEXTURES, none */
};

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
/* Pixels x, y, w by h of colour buffer draw (0 front, 1 back) made black, and their depth 0 with
   depth. */
void tdfx_cleared(Tdfx *c, uint32_t draw, int32_t x, int32_t y, int32_t w, int32_t h, bool depth);
/* clutData's entries: the video output looks a channel up as its 5 or 6 bits with zeros below,
   between two entries. */
enum { TDFX_CLUT = 33 };
/* clutData's entry i: a channel shown as gfx widens it, its top bits repeated below. */
uint32_t tdfx_clut(uint32_t i);
/* The gamma table written: the video running. */
void tdfx_gamma(Tdfx *c);
/* The next frame made the shown one's through the frame buffer's window, pixel by pixel. */
void tdfx_carried(Tdfx *c);
/* The driver's table, its family's: null, the host's ABI another. */
const AccelDriver *tdfx_driver(const AccelHost *host, const TdfxFamily *family);
#endif
