/* plat/dos32/drivers/3dfx: what an SST card is sent, against values worked by hand. */
#include "plat/dos32/drivers/3dfx/dac.h"
#include "plat/dos32/drivers/3dfx/regs.h"
#include "plat/dos32/drivers/3dfx/setup.h"
#include "plat/dos32/drivers/3dfx/space.h"
#include "plat/dos32/drivers/3dfx/sst.h"
#include "plat/dos32/drivers/3dfx/tdfx.h"
#include "unit.h"
#include <stdlib.h>
#include <string.h>

static bool near(const float a, const float b) { return a - b < 1e-4f && b - a < 1e-4f; }

/* Corners given bottom first: sorted top first, moved to the card's pixel centres, red's
   gradients 10 across and 5 down whatever the order. */
TEST(tdfx_setup_sorts_corners_and_finds_gradients)
{
    const AccelVertex v[3] = {
        {0, 10, 0, 1, 0, 0, 0xFF320000},
        {0, 0, 0, 1, 0, 0, 0xFF000000},
        {10, 0, 0, 1, 0, 0, 0xFF640000},
    };

    TdfxTriangle t;

    CHECK(tdfx_setup(v, 0, 0, 1, 255, &t));
    CHECK(near(t.y[0], 0.5f) && near(t.y[2], 10.5f) && near(t.x[2], 0.5f));
    CHECK(near(t.x[0], 0.5f) && near(t.start[TDFX_R], 0));
    CHECK(near(t.dx[TDFX_R], 10) && near(t.dy[TDFX_R], 5));
    CHECK(near(t.start[TDFX_A], 255) && near(t.dx[TDFX_A], 0));
}

/* The same corners mirrored: the card's area's sign the other way. */
TEST(tdfx_setup_signs_a_mirrored_triangle)
{
    const AccelVertex right[3] =
        {{0, 0, 0, 1, 0, 0, 0}, {10, 0, 0, 1, 0, 0, 0}, {0, 10, 0, 1, 0, 0, 0}};
    const AccelVertex left[3] =
        {{0, 0, 0, 1, 0, 0, 0}, {-10, 0, 0, 1, 0, 0, 0}, {0, 10, 0, 1, 0, 0, 0}};

    TdfxTriangle a, b;

    CHECK(tdfx_setup(right, 0, 0, 1, 255, &a) && tdfx_setup(left, 0, 0, 1, 255, &b));
    CHECK(a.command != b.command);
}

/* q of 2 and 4 scaled so the larger is 1; S, T over w with them; alpha times the share. */
TEST(tdfx_setup_scales_w_into_the_cards_range)
{
    const AccelVertex v[3] = {
        {0, 0, 0, 2, 8, 4, 0xFF000000},
        {10, 0, 0, 4, 8, 4, 0xFF000000},
        {0, 10, 0, 2, 8, 4, 0xFF000000},
    };

    TdfxTriangle t;

    CHECK(tdfx_setup(v, 0, 0, 4, 128, &t));
    CHECK(near(t.start[TDFX_W], 0.5f) && near(t.start[TDFX_S], 8 * 4 * 0.5f));
    CHECK(near(t.start[TDFX_T], 4 * 4 * 0.5f) && near(t.dx[TDFX_W], 0.05f));
    CHECK(near(t.start[TDFX_A], 128));
}

/* A keyed texel mixed into the back buffer, without depth, as worked from the register bits. */
TEST(tdfx_words_of_a_keyed_texel_mixed)
{
    const AccelState s = {
        .texture = 1,
        .source = ACCEL_TEXEL,
        .flags = ACCEL_KEY,
        .blend = ACCEL_MIX,
        .alpha = 255,
    };

    const TdfxWords w = tdfx_words(&s, 1);

    CHECK(w.colour_path == 0x0C482405 && w.alpha_mode == 0x5119);
    CHECK(w.fbz_mode == 0x4201 && w.texture_mode == 0x08241BC1);
}

/* Depth tested and written: greater passes; written alone, always. */
TEST(tdfx_words_test_depth_greater_nearer)
{
    const AccelState tested = {
        .source = ACCEL_COLOUR,
        .depth = ACCEL_DEPTH_TEST | ACCEL_DEPTH_WRITE
    };
    const AccelState written =
        {.source = ACCEL_COLOUR, .blend = ACCEL_KEEP, .depth = ACCEL_DEPTH_WRITE};

    CHECK(tdfx_words(&tested, 0).fbz_mode == (0x0001 | 0x0010 | 4 << 5 | 0x0200 | 0x0400));
    CHECK(tdfx_words(&written, 0).fbz_mode == (0x0001 | 0x0010 | 7 << 5 | 0x0400));
}

/* 64 by 64 is level 2 (0x208), past LODs 0 and 1's 128 KB and 32 KB; 256 by 32 level 0, S wider
   by 8. */
TEST(tdfx_textures_are_placed_by_level_and_aspect)
{
    CHECK(tdfx_lod(64, 64) == 0x208 && tdfx_skipped(64, 64) == 131072 + 32768);
    CHECK(tdfx_level(64, 64) == 2 && tdfx_level(256, 32) == 0);
    CHECK(tdfx_lod(256, 32) == (1u << 20 | 3u << 21) && tdfx_skipped(256, 32) == 0);
    CHECK(tdfx_lod(8, 64) == (0x104 * 2 | 3u << 21));
}

/* Ranges given out first fit, aligned; one given back is the first room again. */
TEST(tdfx_space_gives_the_first_room_aligned)
{
    static TdfxSpace s = {.size = 100};

    CHECK(tdfx_take(&s, 10, 1) == 0 && tdfx_take(&s, 10, 16) == 16 && tdfx_take(&s, 5, 1) == 10);
    tdfx_give(&s, 0);
    CHECK(
        tdfx_take(&s, 8, 8) == 0 && tdfx_take(&s, 70, 1) == 26 && tdfx_take(&s, 5, 1) == TDFX_NONE
    );
}

/* The PLLs Glide picks (sst1InitComputeClkParams), worked by its loop: m, n, p and the AT&T's
   filter byte. */
TEST(tdfx_clock_picks_glides_plls)
{
    static const struct {
        float mhz;
        TdfxClock k;
    } WANT[] = {
        {16.0f, {123, 12, 3, 0xA6}},
        {20.093f, {99, 7, 3, 0xA6}},
        {25.175f, {125, 7, 3, 0xA6}},
        {30.0f, {65, 2, 3, 0xA6}},
        {40.0f, {121, 9, 2, 0xC4}},
        {50.0f, {40, 1, 2, 0x84}},
        {80.0f, {121, 9, 1, 0xA8}},
    };

    for (uint32_t i = 0; i < sizeof WANT / sizeof *WANT; i++) {
        TdfxClock k;

        CHECK(tdfx_clock(WANT[i].mhz, &k));
        CHECK(k.m == WANT[i].k.m && k.n == WANT[i].k.n && k.p == WANT[i].k.p);
        CHECK(k.loop == WANT[i].k.loop);
    }

    TdfxClock k;

    CHECK(!tdfx_clock(14.0f, &k));
    CHECK(tdfx_clock(25.175f, &k) && near(tdfx_clock_mhz(&k), 14.318f * 127 / 72));
}

/* What the video output shows of channel bits v, high of 8, through clutData column at (16, 8
   or 0): between the entries either side, as the SST-1 spec and 86Box have it. */
static uint32_t shown(const uint32_t v, const uint32_t at)
{
    const uint32_t lo = tdfx_clut(v >> 3) >> at & 0xFF, hi = tdfx_clut((v >> 3) + 1) >> at & 0xFF;

    return (lo * (8 - (v & 7)) + hi * (v & 7)) >> 3;
}

/* Each 565 channel shows as gfx widens it, its top bits repeated below, but green's top step, 2
   short: the last entry cannot lift the step between it and the one before. A linear table showed
   white as 248. */
TEST(tdfx_gamma_shows_565_as_gfx_widens_it)
{
    for (uint32_t v = 0; v < 32; v++) {
        CHECK(shown(v << 3, 16) == (v << 3 | v >> 2));
        CHECK(shown(v << 3, 0) == (v << 3 | v >> 2));
    }

    for (uint32_t v = 0; v < 63; v++)
        CHECK(shown(v << 2, 8) == (v << 2 | v >> 4));

    CHECK(shown(63 << 2, 8) == 253);
}

/* A mask is the iterated colour, its alpha the texel's times the iterated: keyed by the texel. */
TEST(tdfx_words_of_a_keyed_mask)
{
    const AccelState s = {.source = ACCEL_MASK, .flags = ACCEL_KEY, .texture = 1, .alpha = 255};
    const TdfxWords w = tdfx_words(&s, 1);

    CHECK(w.colour_path == (0x08480004 | 0x04000000));
    CHECK(w.alpha_mode == (1 | 4 << 1));
}

/* ---- the shared driver (tdfx.c) on a card that is memory ---- */

enum { CARD_BYTES = 16 << 20 };

static uint8_t *card_memory;

static uint64_t ACCEL_CALL fake_clock(void) { return 0; }

static bool fake_open(Tdfx *const c, const uint16_t w, const uint16_t h)
{
    card_memory = calloc(1, CARD_BYTES);
    card_memory[SST_STATUS] = SST_ROOM; /* room, nothing busy */
    c->regs = card_memory;
    c->lfb = card_memory + SST_LFB;
    c->lfb_row = 2048;
    c->texels = card_memory + SST_TEXELS;
    c->stride = 640;
    c->rows = 544;
    c->mode_w = w;
    c->mode_h = h;
    c->texture_bytes = 2 << 20;
    return true;
}

static void fake_close(Tdfx *const c)
{
    (void)c;
    free(card_memory);
}

static uint32_t word_at(const uint32_t at)
{
    uint32_t v;

    memcpy(&v, card_memory + at, sizeof v);
    return v;
}

/* A 64 by 64 texture first in memory: its base 160 KB before it, wrapped in TMU0's 4 MB, where
   levels 0 and 1 would be; its rows written to level 2's window, two texels a word. */
TEST(tdfx_driver_places_a_texture_past_the_levels_above_it)
{
    static const TdfxFamily fake = {"fake", &sst_chip, fake_open, fake_close};
    static const AccelHost host =
        {.size = sizeof(AccelHost), .abi = ACCEL_ABI, .clock_us = fake_clock};
    static uint16_t texels[64 * 2];

    const AccelDriver *const d = tdfx_driver(&host, &fake);

    AccelCard card;

    for (uint32_t i = 0; i < 64 * 2; i++)
        texels[i] = (uint16_t)i;

    CHECK(d && d->open(&(PciFunction){}, 320, 240, &card) && card.memory == 2 << 20);
    CHECK(d->texture(1, 64, 64));
    d->upload(1, (AccelRect){0, 5, 64, 2}, texels, 64);
    CHECK(word_at(SST_TMU0 + SST_TEX_BASE) == (((0u - 163840) >> 3) & 0x7FFFF));
    CHECK(word_at(SST_TMU0 + SST_TLOD) == 0x208);
    CHECK(word_at(SST_TEXELS + (2 << 17) + (5 << 9) + (2 << 1)) == (3u << 16 | 2));
    CHECK(word_at(SST_TEXELS + (2 << 17) + (6 << 9) + (62 << 1)) == (127u << 16 | 126));
    d->close();
}

/* The Banshee's and Voodoo 3's (voodoo3.c): a card of 16 MB of SDRAM, its BIOS's VESA mode
   640x480. */
enum : uint32_t {
    H3_BAR0 = 0x10000000,
    H3_BAR1 = 0x20000000,
    H3_REGS = 8 << 20,
    H3_FRAME = 16 << 20,
    H3_3D = 0x200000,
    H3_PORT = 0x600000,
    H3_VID_PROC = 0x5C,
    H3_OVERLAY_END = 0xA0,
    H3_STRIDES = 0xE8,
    H3_LEFT_OVERLAY = 0x250,
    H3_SWAP = 0x128,
    H3_COLOUR_AT = 0x1EC,
    H3_COLOUR_STRIDE = 0x1F0,
    H3_AUX_AT = 0x1F4,
    H3_BUFFER = 2048 * 1280, /* the clip's rows of the mode's pitch */
    H3_FIFO = 256 << 10,     /* the command FIFO's bytes, after the buffers */
    H3_CMD = 0x80000,        /* the command FIFO's registers: its read pointer */
    H3_READ = 0x2C,
};

const AccelDriver *ACCEL_CALL accel_init(const AccelHost *host);

static struct {
    uint8_t *regs, *frame;
    uint64_t us;
    uint32_t ported; /* the words of texels the port took */
    uint16_t w, h;
    uint8_t bits;
} h3;

static uint32_t ACCEL_CALL h3_pci_read(const PciFunction *const card, const uint8_t reg)
{
    (void)card;
    return reg == 0x10 ? H3_BAR0 : reg == 0x14 ? H3_BAR1 : 0;
}

static void ACCEL_CALL
h3_pci_write(const PciFunction *const card, const uint8_t reg, const uint32_t v)
{
    (void)card, (void)reg, (void)v;
}

static volatile uint8_t *ACCEL_CALL h3_map(const uint32_t phys, const uint32_t bytes)
{
    return phys == H3_BAR0 && bytes <= H3_REGS    ? h3.regs
           : phys == H3_BAR1 && bytes <= H3_FRAME ? h3.frame
                                                  : nullptr;
}

static uint32_t h3_word(const uint8_t *const at)
{
    uint32_t v;

    memcpy(&v, at, sizeof v);
    return v;
}

/* The command FIFO's word at *at, taken: cleared, so a zero is a word not written. */
static uint32_t h3_taken(uint32_t *const at)
{
    const uint32_t v = h3_word(h3.frame + *at);

    memset(h3.frame + *at, 0, 4);
    *at += 4;
    return v;
}

static void h3_stored(const uint32_t at, const uint32_t v) { memcpy(h3.regs + at, &v, sizeof v); }

/* The packets the driver wrote to its command FIFO from the read pointer on, done as the card does:
   PKT1's register writes to the 3D window, PKT5's texels to the port, PKT0's jumps; the read
   pointer left past them. */
static void h3_played(void)
{
    uint32_t at = h3_word(h3.regs + H3_CMD + H3_READ);

    while (h3_word(h3.frame + at)) {
        const uint32_t head = h3_taken(&at);

        if ((head & 7) == 1) {
            uint32_t reg = (head & 0x7FF8) >> 1;

            for (uint32_t n = head >> 16; n--; reg += head >> 15 & 1 ? 4 : 0)
                h3_stored(H3_3D + reg, h3_taken(&at));
        } else if ((head & 7) == 5) {
            uint32_t to = h3_taken(&at);

            for (uint32_t n = head >> 3 & 0x7FFFF; n--; to += 4, h3.ported++)
                h3_stored(H3_PORT + to, h3_taken(&at));
        } else if ((head & 0x3F) == 3 << 3) {
            at = head >> 4 & 0xFFFFFC;
        }
    }

    h3_stored(H3_CMD + H3_READ, at);
}

/* A millisecond a read, the card reading its command FIFO as the driver waits. */
static uint64_t ACCEL_CALL h3_clock(void)
{
    h3_played();
    return h3.us += 1000;
}

static bool ACCEL_CALL
h3_vesa(const uint16_t w, const uint16_t h, const uint8_t bits, AccelMode *const out)
{
    h3.w = w, h3.h = h, h3.bits = bits;
    *out = (AccelMode){640, 480, 1280, H3_BAR1};
    return true;
}

static const AccelHost H3_HOST = {
    .size = sizeof(AccelHost),
    .abi = ACCEL_ABI,
    .pci_read = h3_pci_read,
    .pci_write = h3_pci_write,
    .map = h3_map,
    .clock_us = h3_clock,
    .vesa = h3_vesa,
};

/* The driver open at 320x240 on the fake card. */
static const AccelDriver *h3_opened(AccelCard *const card)
{
    const uint32_t sdram = 1u << 30, status = 0x1F | 1u << 6; /* room, idle, not in a retrace */

    h3.regs = calloc(1, H3_REGS);
    h3.frame = calloc(1, H3_FRAME);
    h3.ported = 0;
    memcpy(h3.regs + 0x1C, &sdram, sizeof sdram);
    memcpy(h3.regs + H3_3D + SST_STATUS, &status, sizeof status);

    const AccelDriver *const d = accel_init(&H3_HOST);

    return d && d->open(&(PciFunction){}, 320, 240, card) ? d : nullptr;
}

static void h3_closed(const AccelDriver *const d)
{
    d->close();
    free(h3.regs);
    free(h3.frame);
}

/* Colour buffers 0 and 1 and the depth buffer, each the clip's 2048 rows, then the command FIFO,
   then textures. */
TEST(tdfx_h3_lays_its_buffers_out_in_frame_memory)
{
    AccelCard card;
    const AccelDriver *const d = h3_opened(&card);

    CHECK(d && h3.w == 320 && h3.h == 240 && h3.bits == 16);
    CHECK(card.memory == H3_FRAME - 3 * H3_BUFFER - H3_FIFO);
    h3_played();
    CHECK(h3_word(h3.regs + H3_3D + H3_COLOUR_STRIDE) == 1280);
    CHECK(h3_word(h3.regs + H3_3D + H3_AUX_AT) == 2 * H3_BUFFER);
    CHECK(h3_word(h3.regs + H3_3D + H3_COLOUR_AT) == H3_BUFFER);
    h3_closed(d);
}

/* A 565 overlay over the whole mode, through the CLUT, its rows the mode's pitch, shows buffer 0
   at first. */
TEST(tdfx_h3_shows_an_overlay_over_the_mode)
{
    AccelCard card;
    const AccelDriver *const d = h3_opened(&card);
    const uint32_t on = 1u << 8, bypass = 1u << 11, rgb565 = 1u << 21;

    CHECK(d);
    CHECK((h3_word(h3.regs + H3_VID_PROC) & (on | bypass | 7u << 21)) == (on | rgb565));
    CHECK(h3_word(h3.regs + H3_OVERLAY_END) == (479u << 12 | 639));
    CHECK(h3_word(h3.regs + H3_STRIDES) >> 16 == 1280);
    h3_played();
    CHECK(h3_word(h3.regs + H3_3D + H3_LEFT_OVERLAY) == 0);
    h3_closed(d);
}

/* The overlay shows the buffer drawn, swapped at the retrace with vsync; the screen is drawn in
   the other, a target in buffer 0. */
TEST(tdfx_h3_shows_the_buffer_drawn_and_draws_in_the_other)
{
    AccelCard card;
    const AccelDriver *const d = h3_opened(&card);

    CHECK(d);
    d->show(true);
    h3_played();
    CHECK(h3_word(h3.regs + H3_3D + H3_LEFT_OVERLAY) == H3_BUFFER);
    CHECK(h3_word(h3.regs + H3_3D + H3_SWAP) == 1);
    CHECK(h3_word(h3.regs + H3_3D + H3_COLOUR_AT) == 0);
    d->show(false);
    h3_played();
    CHECK(h3_word(h3.regs + H3_3D + H3_LEFT_OVERLAY) == 0);
    CHECK(h3_word(h3.regs + H3_3D + H3_SWAP) == 0);
    CHECK(h3_word(h3.regs + H3_3D + H3_COLOUR_AT) == H3_BUFFER);
    CHECK(d->target(1, 64, 64, false));
    d->aim(1, (AccelRect){0, 0, 64, 64});
    h3_played();
    CHECK(h3_word(h3.regs + H3_3D + H3_COLOUR_AT) == 0);
    h3_closed(d);
}

/* A 64 by 64 texture first in texture memory: texBaseAddr its byte address less levels 0 and 1,
   its texels through the port from there, a row of 64 after another. */
TEST(tdfx_h3_sends_texels_through_the_port_from_the_base)
{
    static uint16_t texels[64 * 2];

    AccelCard card;
    const AccelDriver *const d = h3_opened(&card);
    const uint32_t skipped = 163840;

    for (uint32_t i = 0; i < 64 * 2; i++)
        texels[i] = (uint16_t)i;

    CHECK(d && d->texture(1, 64, 64));
    d->upload(1, (AccelRect){0, 5, 64, 2}, texels, 64);
    h3_played();
    CHECK(h3_word(h3.regs + H3_3D + SST_TMU0 + SST_TEX_BASE) == 3 * H3_BUFFER + H3_FIFO - skipped);
    CHECK(h3_word(h3.regs + H3_PORT + skipped + (5 * 64 + 2) * 2) == (3u << 16 | 2));
    CHECK(h3_word(h3.regs + H3_PORT + skipped + (6 * 64 + 62) * 2) == (127u << 16 | 126));
    h3_closed(d);
}

/* The screen drawn is buffer 1's, centred in the mode: read through BAR1. */
TEST(tdfx_h3_reads_the_screen_back_through_frame_memory)
{
    AccelCard card;
    const AccelDriver *const d = h3_opened(&card);
    const uint16_t red = 0xF800;

    uint8_t rgba[4];

    CHECK(d);
    memcpy(h3.frame + H3_BUFFER + 120 * 1280 + 160 * 2, &red, sizeof red);
    d->read(ACCEL_SCREEN, (AccelRect){0, 0, 1, 1}, rgba);
    CHECK(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 255);
    h3_closed(d);
}

/* A frame is queued behind the card's draws, not shown once it is idle: waiting cost 310k of
   CUBE's 528k cycles a frame on 86Box's Banshee. */
TEST(tdfx_h3_shows_a_frame_without_waiting_for_the_card)
{
    AccelCard card;
    const AccelDriver *const d = h3_opened(&card);
    const uint32_t drawing = 0x1F | 1u << 6 | 0x380;

    CHECK(d);
    memcpy(h3.regs + H3_3D + SST_STATUS, &drawing, sizeof drawing);
    const uint64_t from = h3.us;

    d->show(false);
    CHECK(h3.us - from < TDFX_WAIT_US);
    h3_closed(d);
}

/* A triangle of a solid colour. */
static const AccelState H3_SOLID = {.source = ACCEL_COLOUR, .alpha = 255};
static const AccelVertex H3_TRIANGLE[3] = {
    {0, 0, 0.5f, 1, 0, 0, 0xFF204060},
    {64, 0, 0.5f, 1, 0, 0, 0xFF204060},
    {0, 64, 0.25f, 1, 0, 0, 0xFF608020},
};

/* Registers go through the command FIFO in frame memory, Glide's way, not the 3D window: written
   there, each waited on the PCI FIFO's 31 entries, 10,000 status reads a frame of CUBE on 86Box. */
TEST(tdfx_h3_writes_registers_through_the_command_fifo)
{
    AccelCard card;
    TdfxTriangle t;
    const AccelDriver *const d = h3_opened(&card);

    CHECK(d && tdfx_setup(H3_TRIANGLE, 160, 120, 1, 255, &t));
    d->state(&H3_SOLID);
    d->triangles(H3_TRIANGLE, 3);
    CHECK(h3_word(h3.regs + H3_3D + SST_FVERTEX) == 0);
    h3_played();
    CHECK(!memcmp(
        h3.regs + H3_3D + SST_FVERTEX,
        (float[]){t.x[0], t.y[0], t.x[1], t.y[1], t.x[2], t.y[2]},
        24
    ));
    CHECK(!memcmp(h3.regs + H3_3D + SST_FSTART, t.start, TDFX_S * 4));
    CHECK(!memcmp(h3.regs + H3_3D + SST_FDX, t.dx, TDFX_S * 4));
    CHECK(!memcmp(h3.regs + H3_3D + SST_FDY, t.dy, TDFX_S * 4));
    CHECK(h3_word(h3.regs + H3_3D + SST_FTRIANGLE) == t.command);
    h3_closed(d);
}

/* The command FIFO goes back to its start by a jump, the driver waiting on the card for room:
   three uploads of 256 by 256, 400 KB through a ring of 256 KB, each arrive whole. */
TEST(tdfx_h3_wraps_its_command_fifo_as_the_card_reads)
{
    static uint16_t texels[256 * 256];

    AccelCard card;
    const AccelDriver *const d = h3_opened(&card);

    CHECK(d && d->texture(1, 256, 256));

    for (uint32_t pass = 0; pass < 3; pass++) {
        for (uint32_t i = 0; i < 256 * 256; i++)
            texels[i] = (uint16_t)(i * 3 + pass);

        d->upload(1, (AccelRect){0, 0, 256, 256}, texels, 256);
    }

    h3_played();
    CHECK(h3.ported == 3 * sizeof texels / 4);
    CHECK(!memcmp(h3.regs + H3_PORT, texels, sizeof texels));
    h3_closed(d);
}

/* The clock is read only once the room last seen in the command FIFO is used up: read for every
   packet, it held CUBE on 86Box's Banshee to 259.9 frames a second against 403.4. */
TEST(tdfx_h3_writes_registers_without_reading_the_clock)
{
    AccelCard card;
    const AccelDriver *const d = h3_opened(&card);

    CHECK(d);
    const uint64_t from = h3.us;

    d->state(&H3_SOLID);
    d->triangles(H3_TRIANGLE, 3);
    CHECK(h3.us == from);
    h3_closed(d);
}
