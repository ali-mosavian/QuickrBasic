/* What the Voodoo Graphics (SST-1) and the Voodoo 2 (CVG) share: their chip, and of their bring-up
 * the FBI's init registers, a mode's timing and frame buffer, and the VGA's screen given back.
 *
 * # How it works
 * A card's frame buffer is sized as Glide sizes it, 1, 2 or 4 MB, its textures taken as 2 MB,
 * every board's least. A mode's frame buffer is two colour buffers and the depth buffer, each a
 * third of the memory a FIFO leaves, up to the rows a clip reaches: the rows past the mode are
 * targets'. fbzMode and lfbMode
 * name the colour buffers front, the one shown, and back; swapbufferCMD swaps them. Texels go
 * through the texture window by level, row and texel, an indexed texture's four a word in order; pixels through the frame buffer's, 1024 a
 * row. A FIFO takes the frame buffer past the buffers: the Voodoo Graphics' memory FIFO, the PCI
 * FIFO's overflow, or, asked for, the Voodoo 2's command FIFO (cvg.h).
 */
#ifndef TDFX_SST_H
#define TDFX_SST_H
#include "plat/dos32/drivers/3dfx/tdfx.h"

/* PCI's: the command register, the revision, BAR0, initEnable, the snoops, the video clock on and
   off. */
enum : uint8_t {
    SST_COMMAND = 0x04,
    SST_REVISION = 0x08,
    SST_BAR0 = 0x10,
    SST_INIT_ENABLE = 0x40,
    SST_SNOOP0 = 0x44,
    SST_SNOOP1 = 0x48,
    SST_VIDEO_ON = 0xC0,
    SST_VIDEO_OFF = 0xE0,
};

enum : uint32_t {
    /* initEnable: fbiInit written, the PCI FIFO written */
    SST_INIT_WRITES = 1,
    SST_FIFO_WRITES = 2,
    /* fbiInit0: the card's screen shown, the graphics and PCI FIFO reset, the FIFO's low-water
       mark, the memory FIFO */
    SST_SHOWN = 1u << 0,
    SST_GRAPHICS_RESET = 1u << 1,
    SST_FIFO_RESET = 1u << 2,
    SST_PCI_LWM_BITS = 0x1Fu << 6,
    SST_PCI_LWM = 13u << 6,
    SST_MEMORY_FIFO = 1u << 13,
    /* fbiInit1: a write's wait state, LFB reads, tiles across over 2, video reset, blank, the
       video's bits */
    SST_WAIT_STATE = 1u << 1,
    SST_LFB_READS = 1u << 3,
    SST_TILES = 4,
    SST_TILES_BITS = 0xFu << 4,
    SST_VIDEO_RESET = 1u << 8,
    SST_BLANK = 1u << 12,
    SST_VIDEO_BITS = 0x7E7FFE00,
    /* fbiInit2: the swap algorithm, a buffer's pages, the DRAM refreshed and how often */
    SST_SWAP_BITS = 3u << 9,
    SST_PAGES = 11,
    SST_PAGES_BITS = 0x1FFu << 11,
    SST_REFRESH = 1u << 22,
    SST_REFRESH_BITS = 0x1FFu << 23,
    SST_BANKED = 1u << 1 | 1u << 20, /* the DRAM banked, as 4 MB is */
    SST_REFRESH_RATE = 0x30u << 23,
    /* fbiInit3: the video FIFO's threshold, the FBI to TMU clock delay */
    SST_VIDEO_FIFO = 1,
    SST_VIDEO_FIFO_BITS = 0x1Fu << 1,
    SST_FT_DELAY = 13,
    SST_FT_BITS = 0xFu << 13,
    /* fbiInit4: LFB reads ahead */
    SST_READ_AHEAD = 1u << 1,
    /* every board's frame buffer and texture memory at least; a card's window's bytes */
    SST_BYTES = 2 << 20,
    SST_WINDOW_BYTES = 16 << 20,
    /* the texture window's: a level, a row, a texel */
    SST_LEVEL_AT = 17,
    SST_ROW_AT = 9,
    SST_TEXEL_AT = 1,
    /* a TMU's trexInit0 and 1, and their values as Glide writes them */
    SST_TREX_INIT0 = 0x31C,
    SST_TREX_INIT1 = 0x320,
    SST_TREX0 = 0x5441,
    SST_TREX1 = 0xF420,
    /* trexInit1: the TMU to FBI clock delay */
    SST_TF_DELAY = 12,
    SST_TF_BITS = 0xFu << 12,
};

/* The Voodoo Graphics': registers, texels and pixels through their windows, as the FIFO has
   room. */
extern const TdfxChip sst_chip;
/* Of it, what the Voodoo 2's shares (TdfxChip). */
uint32_t sst_based(const Tdfx *c, uint32_t at);
uint32_t sst_drawn_in(Tdfx *c, uint32_t buffer);
volatile uint8_t *sst_pixels(Tdfx *c, uint32_t buffer);
void sst_carried(Tdfx *c);
void sst_paletted(Tdfx *c);
/* Whether the Voodoo 2's blitter copies between the colour buffers, tried on two pixels of each:
   DOSBox-X's has none. Once the mode is laid out. */
bool sst_blits(Tdfx *c);

/* A mode, w by h of the frame buffer's: its timing's words, its tiles across (32 pixels each),
   the video FIFO's threshold, its family's flags (scan doubling) and its pixel clock. */
typedef struct {
    uint16_t w, h;
    uint32_t hsync, vsync, back_porch;
    uint8_t tiles, video_fifo, flags;
    float mhz;
} SstMode;

/* The smallest of n modes holding w by h whose buffers fit in bytes: null, none. */
const SstMode *sst_mode_for(
    const SstMode *modes,
    uint32_t n,
    uint16_t w,
    uint16_t h,
    uint32_t bytes
);
/* Init register reg's clear bits cleared and set's set, the FBI idle after. */
void sst_changed(const Tdfx *c, uint32_t reg, uint32_t clear, uint32_t set);
/* A few status reads: what Glide waits after a PCI configuration write. */
void sst_stalled(const Tdfx *c);
/* The graphics unit and the PCI FIFO reset, then let run. */
void sst_reset(const Tdfx *c);
/* The card's memory decoded and mapped, its windows found: false, it cannot be. */
bool sst_mapped(Tdfx *c);
/* The physical address of the card's window: its registers, frame buffer and texels. */
uint32_t sst_window(const Tdfx *c);
/* Mode m's timing written, the video unit in reset. */
void sst_timed(const Tdfx *c, const SstMode *m);
/* A buffer's 4K pages for mode m in a frame buffer of bytes, up to the rows a clip reaches. */
uint32_t sst_pages(const SstMode *m, uint32_t bytes);
/* How a family writes a layout's tiles across, 32 pixels each. */
typedef void SstTiled(const Tdfx *c, uint32_t tiles);
/* Glide's fbiMemSize: the frame buffer's bytes, 4, 2 or 1 MB, by the pixels written in each layout
   that read back; 0, none do. Its pixels are lost; fbiInit0 to 2 are put back. */
uint32_t sst_sized(Tdfx *c, SstTiled *tiled);
/* The frame buffer's first byte past the buffers of pages each. */
uint32_t sst_after_buffers(uint32_t pages);
/* sst1InitVideo's memory FIFO, the frame buffer past the buffers of pages each: the room for
   writes after read from it. */
void sst_queued_in_memory(const Tdfx *c, uint32_t pages);
/* Mode m shown at clock, its buffers pages each: the frame buffer's shape and rate set. */
void sst_laid_out(Tdfx *c, const SstMode *m, uint32_t pages, float clock);
/* The VGA's screen shown again: the video in reset, blank, the refresh off, the units reset;
   fbiInit0's bit 0 shows, the Voodoo's screen. */
void sst_closed(const Tdfx *c, uint32_t shows);
/* Last, as Linux's sstfb: the video clock off and the init registers closed. */
void sst_stopped(const Tdfx *c);
#endif
