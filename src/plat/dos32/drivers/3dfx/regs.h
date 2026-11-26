/* The SST registers 3dfx's cards share: the Voodoo Graphics' (SST-1) and Voodoo 2's at their base,
 * Banshee's and Voodoo 3's 3D at theirs. Offsets in bytes; a register's chip select above them.
 */
#ifndef TDFX_REGS_H
#define TDFX_REGS_H
#include <stdint.h>

enum : uint32_t {
    SST_STATUS = 0x000,
    /* a triangle's, as floats: its corners' x and y, then each parameter's start, x and y
       gradients, SST_PARAMS of them in TdfxParam's order */
    SST_FVERTEX = 0x088,
    SST_FSTART = 0x0A0,
    SST_FDX = 0x0C0,
    SST_FDY = 0x0E0,
    SST_FTRIANGLE = 0x100,
    SST_FBZ_COLOUR_PATH = 0x104,
    SST_FOG_MODE = 0x108,
    SST_ALPHA_MODE = 0x10C,
    SST_FBZ_MODE = 0x110,
    SST_LFB_MODE = 0x114,
    SST_CLIP_X = 0x118,
    SST_CLIP_Y = 0x11C,
    SST_NOP = 0x120,
    SST_FASTFILL = 0x124,
    SST_SWAP = 0x128,
    SST_ZA_COLOUR = 0x130,
    SST_COLOUR1 = 0x148,
    SST_FBI_INIT4 = 0x200,
    SST_BACK_PORCH = 0x208,
    SST_DIMENSIONS = 0x20C,
    SST_FBI_INIT0 = 0x210,
    SST_FBI_INIT1 = 0x214,
    SST_FBI_INIT2 = 0x218,
    SST_FBI_INIT3 = 0x21C,
    SST_HSYNC = 0x220,
    SST_VSYNC = 0x224,
    SST_CLUT = 0x228,
    SST_DAC = 0x22C,
    SST_FBI_INIT5 = 0x244,
    SST_FBI_INIT6 = 0x248,
    SST_FBI_INIT7 = 0x24C,
    SST_TEXTURE_MODE = 0x300,
    SST_TLOD = 0x304,
    SST_TDETAIL = 0x308,
    SST_TEX_BASE = 0x30C,

    /* the chip a write goes to: none, all */
    SST_FBI = 0x400,
    SST_TMU0 = 0x800,
    SST_TMU1 = 0x1000,

    /* the windows: the frame buffer's pixels, and textures' texels */
    SST_LFB = 0x400000,
    SST_TEXELS = 0x800000,
};

/* status: the FIFO's room, and what is busy. */
enum : uint32_t { SST_ROOM = 0x3F, SST_FBI_BUSY = 0x80, SST_BUSY = 0x380 };

/* fbzMode. */
enum : uint32_t {
    SST_CLIPPED = 1u << 0,
    SST_DEPTH = 1u << 4,
    SST_DEPTH_FUNCTION = 5, /* the shift of its 3 bits */
    SST_RGB_WRITE = 1u << 9,
    SST_AUX_WRITE = 1u << 10,
    SST_DRAW_BUFFER = 14, /* the shift: 0 front, 1 back */
};

/* A depth or alpha test's function. */
enum : uint32_t { SST_GREATER = 4, SST_ALWAYS = 7 };

/* fbzColorPath: the texel times the iterated colour, its alpha too; the iterated colour, its
   alpha the texel's times the iterated; the parameters moved to the first pixel's centre. */
enum : uint32_t {
    SST_PATH_TEXEL = 0x08482405,
    SST_PATH_MASK = 0x08480004,
    SST_PATH_ADJUST = 0x04000000,
};

/* alphaMode: the test, its function's shift; blending src times alpha over the rest, or added. */
enum : uint32_t { SST_ALPHA_TEST = 1, SST_ALPHA_FUNCTION = 1, SST_MIX = 0x5110, SST_ADD = 0x4110 };

/* textureMode: in perspective, minified and magnified linearly, S and T clamped, the format's shift
   and ARGB1555's, and TMU0 combining its own texel alone. */
enum : uint32_t {
    SST_PERSPECTIVE = 1u << 0,
    SST_LINEAR = 3u << 1,
    SST_CLAMP_S = 1u << 6,
    SST_CLAMP_T = 1u << 7,
    SST_FORMAT = 8,
    SST_ARGB1555 = 11,
    SST_LOCAL = 0x08241000,
};

/* tLOD: the LOD's shift, 4 a level; S the wider; the aspect's shift. */
enum : uint32_t { SST_LOD_MIN = 0, SST_LOD_MAX = 6, SST_S_WIDER = 1u << 20, SST_ASPECT = 21 };
#endif
