/* plat/dos32/drivers/3dfx: what an SST card is sent, against values worked by hand. */
#include "plat/dos32/drivers/3dfx/cvg.h"
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

    const TdfxWords w = tdfx_words(&s, 1, false);

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

    CHECK(tdfx_words(&tested, 0, false).fbz_mode == (0x0001 | 0x0010 | 4 << 5 | 0x0200 | 0x0400));
    CHECK(tdfx_words(&written, 0, false).fbz_mode == (0x0001 | 0x0010 | 7 << 5 | 0x0400));
}

/* 64 by 64 is level 2 (0x208), past LODs 0 and 1's 128 KB and 32 KB; 256 by 32 level 0, S wider
   by 8. */
TEST(tdfx_textures_are_placed_by_level_and_aspect)
{
    CHECK(tdfx_lod(64, 64) == 0x208 && tdfx_skipped(64, 64, 2) == 131072 + 32768);
    CHECK(tdfx_level(64, 64) == 2 && tdfx_level(256, 32) == 0);
    CHECK(tdfx_lod(256, 32) == (1u << 20 | 3u << 21) && tdfx_skipped(256, 32, 2) == 0);
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
    const TdfxWords w = tdfx_words(&s, 1, false);

    CHECK(w.colour_path == (0x08480004 | 0x04000000));
    CHECK(w.alpha_mode == (1 | 4 << 1));
}

/* An indexed texel alone, keyed: P_8 written in order, index 0 cut by the chroma key, not the
   alpha test; as a mask, the iterated colour, the texel's colour what the key tests. */
TEST(tdfx_words_of_an_indexed_texel_keyed_by_chroma)
{
    const AccelState s = {
        .texture = 1,
        .source = ACCEL_TEXEL,
        .flags = ACCEL_KEY | ACCEL_REPLACE,
        .alpha = 255,
    };
    const AccelState mask = {.texture = 1, .source = ACCEL_MASK, .flags = ACCEL_KEY, .alpha = 255};
    const TdfxWords w = tdfx_words(&s, 1, true), m = tdfx_words(&mask, 1, true);

    CHECK(w.colour_path == (0x08000005 | 0x04000000) && w.alpha_mode == 0);
    CHECK(w.fbz_mode == (0x4201 | 0x0002));
    CHECK(w.texture_mode == (0x08241000 | 5u << 8 | 1u << 31 | 0xC1));
    CHECK(m.colour_path == (0x08824101 | 0x04000000) && m.fbz_mode == w.fbz_mode);
}

/* An indexed texture's levels are a byte a texel: half a 16-bit one's. */
TEST(tdfx_indexed_textures_skip_a_byte_a_texel)
{
    CHECK(tdfx_skipped(64, 64, 1) == (131072 + 32768) / 2);
}

/* ---- the shared driver (tdfx.c) on a card that is memory ---- */

enum { CARD_BYTES = 16 << 20 };

static uint8_t *card_memory;

static uint64_t ACCEL_CALL fake_clock(void) { return 0; }

static uint32_t ACCEL_CALL fake_pci_read(const PciFunction *const card, const uint8_t reg)
{
    (void)card, (void)reg;
    return 0;
}

static void ACCEL_CALL
fake_pci_write(const PciFunction *const card, const uint8_t reg, const uint32_t v)
{
    (void)card, (void)reg, (void)v;
}

static volatile uint8_t *ACCEL_CALL fake_map(const uint32_t phys, const uint32_t bytes)
{
    (void)phys;
    return bytes <= CARD_BYTES ? card_memory : nullptr;
}

static const AccelHost FAKE_HOST = {
    .size = sizeof(AccelHost),
    .abi = ACCEL_ABI,
    .pci_read = fake_pci_read,
    .pci_write = fake_pci_write,
    .map = fake_map,
    .clock_us = fake_clock,
};

static void status_is(const uint32_t v) { memcpy(card_memory + SST_STATUS, &v, sizeof v); }

/* The card mapped as the drivers map it, its status room and nothing busy. */
static bool fake_open(Tdfx *const c, const uint16_t w, const uint16_t h)
{
    card_memory = calloc(1, CARD_BYTES);
    status_is(SST_ROOM);
    sst_mapped(c);
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
    static uint16_t texels[64 * 2];

    const AccelDriver *const d = tdfx_driver(&FAKE_HOST, &fake);

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

/* An indexed 64 by 64 texture: its base 80 KB before it, a row's indices four a word in order
   through level 2's window, the format P_8 written in order. */
TEST(tdfx_driver_sends_indexed_texels_four_a_word)
{
    static const TdfxFamily fake = {"fake", &sst_chip, fake_open, fake_close};
    static uint16_t texels[64];

    const AccelDriver *const d = tdfx_driver(&FAKE_HOST, &fake);

    AccelCard card;

    for (uint32_t i = 0; i < 64; i++)
        texels[i] = (uint16_t)(0x8000 | i);

    CHECK(d && d->open(&(PciFunction){}, 320, 240, &card));
    CHECK(d->indexed(1, 64, 64));
    d->upload(1, (AccelRect){0, 5, 64, 1}, texels, 64);
    CHECK(word_at(SST_TMU0 + SST_TEX_BASE) == (((0u - 81920) >> 3) & 0x7FFFF));
    CHECK((word_at(SST_TMU0 + SST_TEXTURE_MODE) & (0xFu << 8 | 1u << 31)) == (5u << 8 | 1u << 31));
    CHECK(word_at(SST_TEXELS + (2 << 17) + (5 << 9)) == 0x03020100);
    CHECK(word_at(SST_TEXELS + (2 << 17) + (5 << 9) + 60) == 0x3F3E3D3C);
    d->close();
}

/* A triangle of a solid colour. */
static const AccelState SOLID = {.source = ACCEL_COLOUR, .alpha = 255};
static const AccelVertex TRIANGLE[3] = {
    {0, 0, 0.5f, 1, 0, 0, 0xFF204060},
    {64, 0, 0.5f, 1, 0, 0, 0xFF204060},
    {0, 64, 0.25f, 1, 0, 0, 0xFF608020},
};

static uint32_t loaded(const uint8_t *const at)
{
    uint32_t v;

    memcpy(&v, at, sizeof v);
    return v;
}

static void stored_at(uint8_t *const at, const uint32_t v) { memcpy(at, &v, sizeof v); }

/* A fake card's command FIFO: the ring as the CPU writes it, its first byte in frame memory, the
   read pointer, where PKT1's registers and PKT5's texels land, frame memory for the H3's 2D
   engine (null, none); the words of texels taken. */
typedef struct {
    uint8_t *ring, *read, *registers, *texels, *frame;
    uint32_t at, ported;
    uint32_t written[4096][2], writes; /* each register written and its word, in order */
} Fifo;

/* The word at the ring's *at, taken: cleared, so a zero is a word not written. */
static uint32_t taken(const Fifo *const f, uint32_t *const at)
{
    uint8_t *const word = f->ring + (*at - f->at);
    const uint32_t v = loaded(word);

    memset(word, 0, 4);
    *at += 4;
    return v;
}

/* The H3's 2D register reg, as the FIFO left it. */
static uint32_t blit_word(const Fifo *const f, const uint32_t reg)
{
    return loaded(f->registers + 0x2000 + reg);
}

/* The 2D engine's command, as 86Box does it: a copy screen to screen, 16 bits a pixel. */
static void blitted_2d(const Fifo *const f)
{
    const uint32_t from = blit_word(f, 0x34), from_row = blit_word(f, 0x54) & 0x1FFF;
    const uint32_t to = blit_word(f, 0x10), to_row = blit_word(f, 0x14) & 0x1FFF;
    const uint32_t xy = blit_word(f, 0x5C), dxy = blit_word(f, 0x6C), size = blit_word(f, 0x68);

    if (blit_word(f, 0x70) != (1 | 1u << 8 | 0xCCu << 24))
        return;

    for (uint32_t y = 0; y < size >> 16; y++)
        memcpy(
            f->frame + to + ((dxy >> 16) + y) * to_row + (dxy & 0xFFFF) * 2,
            f->frame + from + ((xy >> 16) + y) * from_row + (xy & 0xFFFF) * 2,
            (size & 0xFFFF) * 2
        );
}

/* The packets the driver wrote from the read pointer on, done as the card does: PKT1's register
   writes, PKT5's texels, PKT0's jumps; the read pointer left past them. None, the FIFO not yet
   placed. */
static void played(Fifo *const f)
{
    if (!f->ring)
        return;

    uint32_t at = loaded(f->read);

    while (loaded(f->ring + (at - f->at))) {
        const uint32_t head = taken(f, &at);

        if ((head & 7) == 1) {
            uint32_t reg = (head & 0x7FF8) >> 1;

            for (uint32_t n = head >> 16; n--; reg += head >> 15 & 1 ? 4 : 0) {
                const uint32_t v = taken(f, &at);

                stored_at(f->registers + reg, v);

                if (f->writes < sizeof f->written / sizeof *f->written) {
                    f->written[f->writes][0] = reg;
                    f->written[f->writes++][1] = v;
                }

                if (f->frame && reg == 0x2070)
                    blitted_2d(f);
            }
        } else if ((head & 7) == 5) {
            uint32_t to = taken(f, &at);

            for (uint32_t n = head >> 3 & 0x7FFFF; n--; to += 4, f->ported++)
                stored_at(f->texels + to, taken(f, &at));
        } else if ((head & 0x3F) == 3 << 3) {
            at = head >> 4 & 0xFFFFFC;
        }
    }

    stored_at(f->read, at);
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
    Fifo fifo;
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

static void h3_played(void) { played(&h3.fifo); }

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
    h3.fifo = (Fifo){
        .ring = h3.frame + 3 * H3_BUFFER,
        .read = h3.regs + H3_CMD + H3_READ,
        .registers = h3.regs + H3_3D,
        .texels = h3.regs + H3_PORT,
        .frame = h3.frame,
        .at = 3 * H3_BUFFER,
    };
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
    CHECK(loaded(h3.regs + H3_3D + H3_COLOUR_STRIDE) == 1280);
    CHECK(loaded(h3.regs + H3_3D + H3_AUX_AT) == 2 * H3_BUFFER);
    CHECK(loaded(h3.regs + H3_3D + H3_COLOUR_AT) == H3_BUFFER);
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
    CHECK((loaded(h3.regs + H3_VID_PROC) & (on | bypass | 7u << 21)) == (on | rgb565));
    CHECK(loaded(h3.regs + H3_OVERLAY_END) == (479u << 12 | 639));
    CHECK(loaded(h3.regs + H3_STRIDES) >> 16 == 1280);
    h3_played();
    CHECK(loaded(h3.regs + H3_3D + H3_LEFT_OVERLAY) == 0);
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
    CHECK(loaded(h3.regs + H3_3D + H3_LEFT_OVERLAY) == H3_BUFFER);
    CHECK(loaded(h3.regs + H3_3D + H3_SWAP) == 1);
    CHECK(loaded(h3.regs + H3_3D + H3_COLOUR_AT) == 0);
    d->show(false);
    h3_played();
    CHECK(loaded(h3.regs + H3_3D + H3_LEFT_OVERLAY) == 0);
    CHECK(loaded(h3.regs + H3_3D + H3_SWAP) == 0);
    CHECK(loaded(h3.regs + H3_3D + H3_COLOUR_AT) == H3_BUFFER);
    CHECK(d->target(1, 64, 64, false));
    d->aim(1, (AccelRect){0, 0, 64, 64});
    h3_played();
    CHECK(loaded(h3.regs + H3_3D + H3_COLOUR_AT) == 0);
    h3_closed(d);
}

/* The frame shown is carried to the next by the 2D engine, through the command FIFO: the CPU
   copied it through BAR1, the card idle first. */
TEST(tdfx_h3_carries_a_frame_on_the_card)
{
    AccelCard card;
    const AccelDriver *const d = h3_opened(&card);
    const uint16_t red = 0xF800;
    const uint32_t first = 120 * 1280 + 160 * 2, last = (120 + 239) * 1280 + (160 + 319) * 2;

    CHECK(d);
    memcpy(h3.frame + H3_BUFFER + first, &red, sizeof red);
    memcpy(h3.frame + H3_BUFFER + last, &red, sizeof red);
    d->show(false);
    d->carry();
    CHECK(loaded(h3.frame + first) == 0);
    h3_played();
    CHECK(!memcmp(h3.frame + first, &red, 2) && !memcmp(h3.frame + last, &red, 2));
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
    CHECK(loaded(h3.regs + H3_3D + SST_TMU0 + SST_TEX_BASE) == 3 * H3_BUFFER + H3_FIFO - skipped);
    CHECK(loaded(h3.regs + H3_PORT + skipped + (5 * 64 + 2) * 2) == (3u << 16 | 2));
    CHECK(loaded(h3.regs + H3_PORT + skipped + (6 * 64 + 62) * 2) == (127u << 16 | 126));
    h3_closed(d);
}

/* The palette eight entries a run to nccTable0's I and Q, an entry's index less its low bit above
   its colour. Index 0's colour is the chroma key: its own top bits, low bits no other colour has.
   Index 1 takes the first low bits tried. */
TEST(tdfx_h3_palettes_index_0_by_a_colour_no_other_has)
{
    static uint32_t colours[256];

    AccelCard card;
    const AccelDriver *const d = h3_opened(&card);

    for (uint32_t i = 0; i < 256; i++)
        colours[i] = i * 0x010101;

    colours[0] = 0x804020;
    colours[1] = 0x804020 | 0x070307;
    h3.fifo.writes = 0;
    d->palette(colours);
    h3_played();

    uint32_t entries = 0, key = 0;

    for (uint32_t k = 0; k < h3.fifo.writes; k++) {
        const uint32_t reg = h3.fifo.written[k][0], v = h3.fifo.written[k][1];

        if (reg >= SST_TMU0 + SST_PALETTE && reg < SST_TMU0 + SST_PALETTE + 32) {
            const uint32_t at = (v >> 23 & 0xFE) | (reg - SST_TMU0 - SST_PALETTE) / 4 % 2;

            CHECK(v >> 31 && at == entries);
            CHECK(at ? (v & 0xFFFFFF) == colours[at] : (v & 0xF8FCF8) == 0x804020);
            key = at ? key : v & 0xFFFFFF;
            entries++;
        }
    }

    CHECK(entries == 256 && key != colours[1]);
    CHECK(loaded(h3.regs + H3_3D + SST_CHROMA_KEY) == key);
    h3_closed(d);
}

/* After the palette the texture cache is flushed, texBaseAddr moved off the texture placed and
   back: 86Box's drew an indexed texture as index 0 in 2 of 4 runs without. */
TEST(tdfx_h3_flushes_its_texture_cache_after_the_palette)
{
    static uint16_t texels[64];
    static uint32_t colours[256];

    AccelCard card;
    const AccelDriver *const d = h3_opened(&card);

    CHECK(d && d->indexed(1, 64, 64));
    d->upload(1, (AccelRect){0, 0, 64, 1}, texels, 64);
    h3_played();
    const uint32_t base = loaded(h3.regs + H3_3D + SST_TMU0 + SST_TEX_BASE);

    h3.fifo.writes = 0;
    d->palette(colours);
    h3_played();

    uint32_t moved = 0, back = 0;

    for (uint32_t k = 0; k < h3.fifo.writes; k++)
        if (h3.fifo.written[k][0] == SST_TMU0 + SST_TEX_BASE) {
            moved += h3.fifo.written[k][1] != base;
            back = h3.fifo.written[k][1] == base;
        }

    CHECK(moved && back);
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

/* Registers go through the command FIFO in frame memory, Glide's way, not the 3D window: written
   there, each waited on the PCI FIFO's 31 entries, 10,000 status reads a frame of CUBE on 86Box. */
TEST(tdfx_h3_writes_registers_through_the_command_fifo)
{
    AccelCard card;
    TdfxTriangle t;
    const AccelDriver *const d = h3_opened(&card);

    CHECK(d && tdfx_setup(TRIANGLE, 160, 120, 1, 255, &t));
    d->state(&SOLID);
    d->triangles(TRIANGLE, 3);
    CHECK(loaded(h3.regs + H3_3D + SST_FVERTEX) == 0);
    h3_played();
    CHECK(!memcmp(
        h3.regs + H3_3D + SST_FVERTEX,
        (float[]){t.x[0], t.y[0], t.x[1], t.y[1], t.x[2], t.y[2]},
        24
    ));
    CHECK(!memcmp(h3.regs + H3_3D + SST_FSTART, t.start, TDFX_S * 4));
    CHECK(!memcmp(h3.regs + H3_3D + SST_FDX, t.dx, TDFX_S * 4));
    CHECK(!memcmp(h3.regs + H3_3D + SST_FDY, t.dy, TDFX_S * 4));
    CHECK(loaded(h3.regs + H3_3D + SST_FTRIANGLE) == t.command);
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
    CHECK(h3.fifo.ported == 3 * sizeof texels / 4);
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

    d->state(&SOLID);
    d->triangles(TRIANGLE, 3);
    CHECK(h3.us == from);
    h3_closed(d);
}

/* The memory FIFO past buffers of 150 pages in 2 MB, as Glide's 640x480 table has it: 31,232
   entries, fbiInit0's high-water mark 0x430, its rows from 450 to the last. */
static bool memory_fifo_open(Tdfx *const c, const uint16_t w, const uint16_t h)
{
    fake_open(c, w, h);
    status_is(0xFFFFu << 12 | SST_ROOM); /* both FIFOs empty */
    sst_queued_in_memory(c, 150);
    return true;
}

TEST(tdfx_sst_queues_in_frame_memory_as_glide_does)
{
    static const TdfxFamily fake = {"fake", &sst_chip, memory_fifo_open, fake_close};

    const AccelDriver *const d = tdfx_driver(&FAKE_HOST, &fake);

    AccelCard card;

    CHECK(d && d->open(&(PciFunction){}, 320, 240, &card));
    CHECK((word_at(SST_FBI_INIT0) >> 14 & 0x7FF) == 0x430 && word_at(SST_FBI_INIT0) & 1u << 13);
    CHECK((word_at(SST_FBI_INIT4) >> 8 & 0x3FF) == 450);
    CHECK((word_at(SST_FBI_INIT4) >> 18 & 0x3FF) == 0x1FF);
    d->close();
}

/* Writes go by the memory FIFO's room, as Glide's _grReCacheFifo: the PCI FIFO full, the memory
   FIFO empty, ten triangles are written, more than the PCI FIFO's 63 entries; by the PCI FIFO's
   room, they waited for ever. */
TEST(tdfx_sst_writes_by_the_memory_fifos_room)
{
    static const TdfxFamily fake = {"fake", &sst_chip, memory_fifo_open, fake_close};
    static AccelVertex ten[10 * 3];

    const AccelDriver *const d = tdfx_driver(&FAKE_HOST, &fake);

    AccelCard card;

    for (uint32_t i = 0; i < 10 * 3; i++)
        ten[i] = TRIANGLE[i % 3];

    CHECK(d && d->open(&(PciFunction){}, 320, 240, &card));
    status_is(0xFFFFu << 12);
    d->state(&SOLID);
    d->triangles(ten, 10 * 3);
    CHECK(loaded(card_memory + SST_FVERTEX) != 0);
    d->close();
}

/* ---- the Voodoo 2's command FIFO (cvg.c), past buffers of 160 pages ---- */

enum : uint32_t { CVG_AT = 3 * 160 * 4096, CVG_WINDOW = 0x200000, CVG_PAGES = 0x1E0 };

static Fifo cvg_fifo;
static uint64_t cvg_us;
static bool cvg_asked; /* SSTV2_CMDFIFO set, to "1" */

static uint32_t ACCEL_CALL cvg_env(const char *const name, char *const out, const uint32_t cap)
{
    (void)out, (void)cap;
    return cvg_asked && !strcmp(name, "SSTV2_CMDFIFO");
}

/* A millisecond a read, the card reading its command FIFO as the driver waits. */
static uint64_t ACCEL_CALL cvg_clock(void)
{
    played(&cvg_fifo);
    return cvg_us += 1000;
}

static bool cvg_open(Tdfx *const c, const uint16_t w, const uint16_t h)
{
    fake_open(c, w, h);
    cvg_queued(c, 160);

    if (cvg_asked)
        cvg_fifo = (Fifo){
            .ring = card_memory + CVG_WINDOW,
            .read = card_memory + 0x1E8,
            .registers = card_memory,
            .texels = card_memory + SST_TEXELS,
            .at = CVG_AT,
        };
    return true;
}

static void cvg_close(Tdfx *const c)
{
    cvg_unqueued(c);
    cvg_fifo = (Fifo){};
    fake_close(c);
}

/* The fake Voodoo 2 open, its command FIFO asked for or not. */
static const AccelDriver *cvg_opened(AccelCard *const card, const bool asked)
{
    static const TdfxFamily family = {"fake Voodoo 2", &sst_chip, cvg_open, cvg_close};
    static const AccelHost host = {
        .size = sizeof(AccelHost),
        .abi = ACCEL_ABI,
        .pci_read = fake_pci_read,
        .pci_write = fake_pci_write,
        .map = fake_map,
        .clock_us = cvg_clock,
        .env = cvg_env,
    };

    cvg_asked = asked;
    const AccelDriver *const d = tdfx_driver(&host, &family);

    return d && d->open(&(PciFunction){}, 320, 240, card) ? d : nullptr;
}

/* The ring, 128 KB past the three buffers, on as Glide's sst1InitCmdFifoDirect turns it on: in
   memory past the buffers, holes counted. */
TEST(tdfx_cvg_places_its_command_fifo_past_the_buffers)
{
    AccelCard card;
    const AccelDriver *const d = cvg_opened(&card, true);

    CHECK(d);
    CHECK(word_at(CVG_PAGES) == ((CVG_AT + (128 << 10) - 1) / 4096 << 16 | CVG_AT / 4096));
    CHECK((word_at(SST_FBI_INIT7) & 7u << 8) == 3u << 8);
    CHECK(!(word_at(SST_FBI_INIT0) & 1u << 13));
    d->close();
    CHECK(!(word_at(SST_FBI_INIT7) & 1u << 8));
}

/* Unless SSTV2_CMDFIFO asks for the command FIFO, registers go straight to the 3D window and the
   FIFO stays off: 86Box shows a fifth fewer frames through it. */
TEST(tdfx_cvg_writes_directly_unless_asked_for_its_command_fifo)
{
    AccelCard card;
    TdfxTriangle t;
    const AccelDriver *const d = cvg_opened(&card, false);

    CHECK(d && tdfx_setup(TRIANGLE, 0, 0, 1, 255, &t));
    CHECK(!(word_at(SST_FBI_INIT7) & 1u << 8));
    d->state(&SOLID);
    d->triangles(TRIANGLE, 3);
    CHECK(!memcmp(
        card_memory + SST_FVERTEX,
        (float[]){t.x[0], t.y[0], t.x[1], t.y[1], t.x[2], t.y[2]},
        24
    ));
    d->close();
}

/* Registers go through the command FIFO, Glide 3's way on the Voodoo 2, not the 3D window. */
TEST(tdfx_cvg_writes_registers_through_the_command_fifo)
{
    AccelCard card;
    TdfxTriangle t;
    const AccelDriver *const d = cvg_opened(&card, true);

    CHECK(d && tdfx_setup(TRIANGLE, 0, 0, 1, 255, &t));
    d->state(&SOLID);
    d->triangles(TRIANGLE, 3);
    CHECK(word_at(SST_FVERTEX) == 0);
    played(&cvg_fifo);
    CHECK(!memcmp(
        card_memory + SST_FVERTEX,
        (float[]){t.x[0], t.y[0], t.x[1], t.y[1], t.x[2], t.y[2]},
        24
    ));
    CHECK(!memcmp(card_memory + SST_FSTART, t.start, TDFX_S * 4));
    CHECK(word_at(SST_FTRIANGLE) == t.command);
    d->close();
}

/* Texels go in packets to texture space at the addresses the texture window takes them: the same
   words as the Voodoo Graphics' direct writes. */
TEST(tdfx_cvg_sends_texels_to_texture_space)
{
    static uint16_t texels[64 * 2];

    AccelCard card;
    const AccelDriver *const d = cvg_opened(&card, true);

    for (uint32_t i = 0; i < 64 * 2; i++)
        texels[i] = (uint16_t)i;

    CHECK(d && d->texture(1, 64, 64));
    d->upload(1, (AccelRect){0, 5, 64, 2}, texels, 64);
    CHECK(word_at(SST_TEXELS + (2 << 17) + (5 << 9) + (2 << 1)) == 0);
    played(&cvg_fifo);
    CHECK(word_at(SST_TEXELS + (2 << 17) + (5 << 9) + (2 << 1)) == (3u << 16 | 2));
    CHECK(word_at(SST_TEXELS + (2 << 17) + (6 << 9) + (62 << 1)) == (127u << 16 | 126));
    d->close();
}

/* A frame's swap goes through the command FIFO, behind the draws before it. */
TEST(tdfx_cvg_shows_through_the_command_fifo)
{
    AccelCard card;
    const AccelDriver *const d = cvg_opened(&card, true);

    CHECK(d);
    d->show(true);
    CHECK(word_at(SST_SWAP) == 0);
    played(&cvg_fifo);
    CHECK(word_at(SST_SWAP) == 1);
    d->close();
}

/* The fake Voodoo 2 laid out in buffers of 160 pages, 20 tiles across, 640x480. */
static void laid_out(Tdfx *const c, const uint16_t w, const uint16_t h)
{
    static const SstMode mode = {640, 480, 1, 1, 0, 20, 0, 0, 25.175f};

    fake_open(c, w, h);
    sst_laid_out(c, &mode, 160, 25.175f);
}

/* Its blitter found, as on 86Box. */
static bool blitter_open(Tdfx *const c, const uint16_t w, const uint16_t h)
{
    laid_out(c, w, h);
    c->blits = true;
    return true;
}

static bool blitter_found; /* what sst_blits found */

/* Its blitter tried. */
static bool tried_open(Tdfx *const c, const uint16_t w, const uint16_t h)
{
    laid_out(c, w, h);
    c->blits = blitter_found = sst_blits(c);
    return true;
}

static bool blitted_at(const uint32_t reg) { return word_at(reg) != 0; }

/* A frame is carried by the blitter, in order with the draws: buffer 1's screen, shown, copied to
   buffer 0, at 160 pages a buffer and 20 tiles a row, the screen 320 by 240 centred in 640x480.
   The CPU copied it through the frame buffer's window, as the Voodoo 1 still does: 68% of
   MEADOW16's frame on 86Box. */
TEST(tdfx_cvg_carries_a_frame_by_its_blitter)
{
    static const TdfxFamily family = {"fake Voodoo 2", &sst_chip, blitter_open, fake_close};

    AccelCard card;
    const AccelDriver *const d = tdfx_driver(&FAKE_HOST, &family);

    CHECK(d && d->open(&(PciFunction){}, 320, 240, &card));
    d->show(false);
    d->carry();
    CHECK(word_at(0x2C0) == 160 && word_at(0x2C4) == 0 && word_at(0x2C8) == (20u << 16 | 20));
    CHECK(word_at(0x2E0) == (120u << 16 | 160) && word_at(0x2E4) == (120u << 16 | 160));
    CHECK(word_at(0x2E8) == (239u << 16 | 319) && word_at(0x2EC) == 0xCCCC);
    CHECK(word_at(0x2F8) == (3u << 14 | 1u << 31));
    d->close();
}

/* A blitter that copies nothing, as DOSBox-X's Voodoo 2, is found missing: the CPU carries. */
TEST(tdfx_cvg_carries_by_the_cpu_without_a_blitter)
{
    static const TdfxFamily family = {"fake Voodoo 2", &sst_chip, tried_open, fake_close};

    AccelCard card;
    const AccelDriver *const d = tdfx_driver(&FAKE_HOST, &family);

    CHECK(d && d->open(&(PciFunction){}, 320, 240, &card) && !blitter_found);
    memset(card_memory + 0x2C0, 0, 0x40);
    d->show(false);
    d->carry();
    CHECK(!blitted_at(0x2C0) && !blitted_at(0x2E8) && !blitted_at(0x2F8));
    d->close();
}

/* 640x480 in 2 MB keeps rows for the scene's 70 by 50 target past the screen: a FIFO of 128 KB
   left 32, and the target did not fit. */
TEST(tdfx_sst_keeps_rows_for_targets_past_the_fifo)
{
    const SstMode mode = {640, 480, 0, 0, 0, 20, 23, 0, 25.175f};

    CHECK(sst_pages(&mode) * 4096 / (640 * 2) >= 480 + 50);
}
