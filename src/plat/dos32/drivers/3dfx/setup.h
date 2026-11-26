/* What an SST card is sent for a triangle and its state, worked out on the CPU: pure, no card.
 *
 * # How it works
 * The card draws a triangle from its corners A, B and C, A the top, and each parameter's value at A
 * and gradients, moving them to the first pixel's centre itself; its pixels' centres are at halves.
 * S and T are a texture's texels at LOD 0, 256 across, each over w, and W is 1 over w: the card
 * divides back per pixel. W is scaled to at most 1, the card's range, which the division undoes.
 */
#ifndef TDFX_SETUP_H
#define TDFX_SETUP_H
#include "plat/dos32/accel/driver.h"

/* A triangle's parameters, in the card's float registers' order. */
typedef enum { TDFX_R, TDFX_G, TDFX_B, TDFX_Z, TDFX_A, TDFX_S, TDFX_T, TDFX_W, TDFX_PARAMS } TdfxParam;

typedef struct {
    float x[3], y[3]; /* A, B, C */
    float start[TDFX_PARAMS], dx[TDFX_PARAMS], dy[TDFX_PARAMS];
    uint32_t command; /* triangleCMD's: the area's sign */
} TdfxTriangle;

/* The registers' words for a state: its texture's place aside. */
typedef struct {
    uint32_t colour_path, alpha_mode, fbz_mode, texture_mode;
} TdfxWords;

/* v's triangle moved by ox, oy, its texels times scale, its alpha times share over 255: false, no
   area. */
bool tdfx_setup(
    const AccelVertex v[3],
    float ox,
    float oy,
    float scale,
    uint8_t share,
    TdfxTriangle *out
);
/* s's words, drawn into buffer draw (0 front, 1 back). */
TdfxWords tdfx_words(const AccelState *s, uint32_t draw);
/* A w by h texture's tLOD: its sides powers of two up to 256, the longer at most 8 times the
   shorter. */
uint32_t tdfx_lod(uint16_t w, uint16_t h);
/* Its level: 0 its longer side 256, 8 one texel; its texels times 1 << level are LOD 0's. */
uint32_t tdfx_level(uint16_t w, uint16_t h);
/* The bytes LODs 0 up to its own of its aspect would take: its base is its texels' place less
   these, as the card finds a level past the larger ones. */
uint32_t tdfx_skipped(uint16_t w, uint16_t h);
#endif
