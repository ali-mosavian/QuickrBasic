/* What an SST card is sent for a triangle and its state, worked out on the CPU: pure, no card.
 *
 * # How it works
 * The card draws a triangle from its corners A, B and C, A the top, and each parameter's value at A
 * and gradients, moving them to the first pixel's centre itself; its pixels' centres are at halves.
 * S and T are a texture's texels at LOD 0, 256 across, each over w, and W is 1 over w: the card
 * divides back per pixel. W is scaled to at most 1, the card's range, which the division undoes.
 * Only the parameters a state's path reads are worked out and sent: the card keeps the rest.
 */
#ifndef TDFX_SETUP_H
#define TDFX_SETUP_H
#include "plat/dos32/accel/driver.h"

/* A triangle's parameters, in the card's float registers' order. */
typedef enum {
    TDFX_R,
    TDFX_G,
    TDFX_B,
    TDFX_Z,
    TDFX_A,
    TDFX_S,
    TDFX_T,
    TDFX_W,
    TDFX_PARAMS
} TdfxParam;

/* A triangle as the card's registers from fvertex take it, a word each: A's, B's and C's x and y;
   each parameter's start, then its x gradient, then its y gradient; triangleCMD's, the area's
   sign. */
enum {
    TDFX_CORNERS = 6,
    TDFX_START = TDFX_CORNERS,
    TDFX_DX = TDFX_START + TDFX_PARAMS,
    TDFX_DY = TDFX_DX + TDFX_PARAMS,
    TDFX_COMMAND = TDFX_DY + TDFX_PARAMS,
    TDFX_WORDS,
};

typedef struct {
    uint32_t word[TDFX_WORDS];
} TdfxTriangle;

/* What a draw's triangles share: their move; their texels' scale; the parameters sent, a bit
   each; their alpha's share over 255. */
typedef struct {
    float ox, oy, scale;
    uint32_t params;
    uint8_t share;
} TdfxDraw;

/* The registers' words for a state: its texture's place aside. */
typedef struct {
    uint32_t colour_path, alpha_mode, fbz_mode, texture_mode;
} TdfxWords;

/* The parameters s's path reads, a bit each: a texture's when texel. */
uint32_t tdfx_params(const AccelState *s, bool texel);
/* A triangle's words sent for params, a bit each: the corners, the parameters' and the command. */
uint32_t tdfx_sent(uint32_t params);
/* v's triangle as d draws it, the words tdfx_sent picks: false, no area. */
bool tdfx_setup(const AccelVertex v[3], const TdfxDraw *d, TdfxTriangle *out);
/* s's words, drawn into buffer draw (0 front, 1 back), its texture indexed or not: an indexed one
   keyed by the chroma key, its index 0's colour. */
TdfxWords tdfx_words(const AccelState *s, uint32_t draw, bool indexed);
/* A w by h texture's tLOD: its sides powers of two up to 256, the longer at most 8 times the
   shorter. */
uint32_t tdfx_lod(uint16_t w, uint16_t h);
/* Its level: 0 its longer side 256, 8 one texel; its texels times 1 << level are LOD 0's. */
uint32_t tdfx_level(uint16_t w, uint16_t h);
/* The bytes LODs 0 up to its own of its aspect would take, texel bytes a texel: its base is its
   texels' place less these, as the card finds a level past the larger ones. */
uint32_t tdfx_skipped(uint16_t w, uint16_t h, uint32_t texel);
/* Four indexed texels' indices, their low bytes, as a word takes them: the first lowest. */
uint32_t tdfx_indices(const uint16_t *texels);
#endif
