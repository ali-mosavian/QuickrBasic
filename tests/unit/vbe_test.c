/* plat/dos32/vbe.c: VBE's modes read, picked and filled, as gfx's DOS screen uses them. */
#include "plat/dos32/vbe.h"
#include "unit.h"
#include <string.h>

/* A 4F01h block: attributes, size, pitch, bits, model; direct colour's masks; the frame buffer. */
static void block(
    uint8_t info[VBE_INFO_BYTES],
    const uint16_t attributes,
    const uint16_t w,
    const uint16_t h,
    const uint16_t pitch,
    const uint8_t bits,
    const uint8_t model,
    const uint8_t masks[6]
)
{
    memset(info, 0, VBE_INFO_BYTES);
    info[0] = (uint8_t)attributes, info[1] = (uint8_t)(attributes >> 8);
    info[0x10] = (uint8_t)pitch, info[0x11] = (uint8_t)(pitch >> 8);
    info[0x12] = (uint8_t)w, info[0x13] = (uint8_t)(w >> 8);
    info[0x14] = (uint8_t)h, info[0x15] = (uint8_t)(h >> 8);
    info[0x19] = bits;
    info[0x1B] = model;

    if (masks)
        memcpy(info + 0x1F, masks, 6);

    info[0x2B] = 0xE0; /* the frame buffer at E0000000h */
}

TEST(vbe_reads_the_modes_gfx_shows_in)
{
    (void)S;
    enum : uint16_t { LINEAR = 0x9B, BANKED = 0x1B, TEXT = 0x0B };

    static const uint8_t x555[6] = {5, 10, 5, 5, 5, 0}, rgb565[6] = {5, 11, 6, 5, 5, 0};

    static const uint8_t x888[6] = {8, 16, 8, 8, 8, 0}, bgr888[6] = {8, 0, 8, 8, 8, 16};

    uint8_t info[VBE_INFO_BYTES];
    VbeMode m;

    block(info, LINEAR, 640, 480, 640, 8, 4, nullptr);
    CHECK(vbe_mode(0x101, VBE_V2, info, &m) && m.depth == 8 && m.linear);
    CHECK(m.w == 640 && m.h == 480 && m.pitch == 640 && m.base == 0xE0000000);

    block(info, LINEAR, 640, 480, 1280, 16, 6, x555);
    CHECK(vbe_mode(0x110, VBE_V2, info, &m) && m.depth == 15);
    block(info, LINEAR, 640, 480, 1280, 16, 6, rgb565);
    CHECK(vbe_mode(0x111, VBE_V2, info, &m) && m.depth == 16);
    block(info, LINEAR, 640, 480, 2560, 32, 6, x888);
    CHECK(vbe_mode(0x112, VBE_V2, info, &m) && m.depth == 32 && m.pitch == 2560);
    block(info, LINEAR, 640, 480, 2560, 32, 6, bgr888);
    CHECK(!vbe_mode(0x112, VBE_V2, info, &m)); /* not &HAARRGGBB's order */
    block(info, LINEAR, 640, 480, 1920, 24, 6, x888);
    CHECK(!vbe_mode(0x112, VBE_V2, info, &m)); /* 24 bits: none of gfx's */
    block(info, TEXT, 132, 25, 264, 0, 0, nullptr);
    CHECK(!vbe_mode(0x109, VBE_V2, info, &m));

    /* VBE 1.2: its window, A's when it writes, else B's */
    block(info, LINEAR, 640, 480, 640, 8, 4, nullptr);
    info[2] = 7, info[4] = 64, info[6] = 64, info[8] = 0x00, info[9] = 0xA0;
    CHECK(vbe_mode(0x101, 0x102, info, &m) && !m.linear && m.window == 0);
    CHECK(m.window_seg == 0xA000 && m.granule == 65536 && m.window_bytes == 65536);
    info[2] = 3, info[3] = 5, info[10] = 0x00, info[11] = 0xB0;
    CHECK(vbe_mode(0x101, 0x102, info, &m) && m.window == 1 && m.window_seg == 0xB000);
    block(info, BANKED, 640, 480, 640, 8, 4, nullptr);
    CHECK(!vbe_mode(0x101, VBE_V2, info, &m)); /* no frame buffer and no window */
    block(info, BANKED, 640, 480, 1280, 16, 6, nullptr);
    info[2] = 7, info[4] = 4, info[6] = 64, info[9] = 0xA0;
    CHECK(vbe_mode(0x111, 0x102, info, &m) && m.depth == 16 && m.granule == 4096); /* no masks */
}

TEST(vbe_picks_the_mode_a_canvas_fills)
{
    (void)S;
    const VbeMode modes[] = {
        vbe_mode_13h(),
        {.number = 0x100, .w = 640, .h = 400, .pitch = 640, .depth = 8},
        {.number = 0x101, .w = 640, .h = 480, .pitch = 640, .depth = 8},
        {.number = 0x111, .w = 640, .h = 480, .pitch = 1280, .depth = 16},
        {.number = 0x103, .w = 800, .h = 600, .pitch = 800, .depth = 8},
    };

    const uint32_t n = sizeof modes / sizeof modes[0];

    CHECK(vbe_pick(modes, n, 320, 200, 8) == 0);
    CHECK(vbe_pick(modes, n, 640, 400, 8) == 1);
    CHECK(vbe_pick(modes, n, 320, 240, 8) == 2); /* doubled, it fills 640x480 */
    CHECK(vbe_pick(modes, n, 400, 300, 8) == 4);
    CHECK(vbe_pick(modes, n, 320, 200, 16) == 3);
    CHECK(vbe_pick(modes, n, 1024, 768, 8) == -1);
    CHECK(vbe_pick(modes, n, 320, 200, 32) == -1);

    const VbeFit f = vbe_fit(&modes[2], 320, 200);
    CHECK(f.k == 2 && f.x == 0 && f.y == 40);
    const VbeFit g = vbe_fit(&modes[4], 640, 480);
    CHECK(g.k == 1 && g.x == 80 && g.y == 60);
    CHECK(!vbe_fit(&modes[0], 321, 200).k);
}

/* An accelerator draws at the canvas's own size: the smallest linear mode holding it. */
TEST(vbe_holds_a_canvas_in_the_smallest_linear_mode)
{
    (void)S;
    const VbeMode modes[] = {
        {.number = 0x10E, .w = 320, .h = 200, .pitch = 640, .depth = 16},
        {.number = 0x114, .w = 800, .h = 600, .pitch = 1600, .depth = 16, .linear = true},
        {.number = 0x111, .w = 640, .h = 480, .pitch = 1280, .depth = 16, .linear = true},
        {.number = 0x110, .w = 640, .h = 480, .pitch = 1280, .depth = 15, .linear = true},
    };

    const uint32_t n = sizeof modes / sizeof modes[0];

    CHECK(vbe_holding(modes, n, 320, 200, 16) == 2); /* not the banked one */
    CHECK(vbe_holding(modes, n, 640, 480, 16) == 2);
    CHECK(vbe_holding(modes, n, 640, 481, 16) == 1);
    CHECK(vbe_holding(modes, n, 801, 600, 16) == -1);
    CHECK(vbe_holding(modes, n, 320, 240, 15) == 3);
}

TEST(vbe_widens_rows_and_splits_them_at_windows)
{
    (void)S;
    const uint8_t row[] = {1, 2, 3, 4};

    uint8_t out[12];

    vbe_widen(out, row, 2, 2, 3);
    CHECK(!memcmp(out, (const uint8_t[]){1, 2, 1, 2, 1, 2, 3, 4, 3, 4, 3, 4}, 12));

    const VbeMode m = {.granule = 4096, .window_bytes = 65536};

    uint32_t step, in;

    CHECK(vbe_window(&m, 100, 640, &step, &in) == 640 && step == 0 && in == 100);
    CHECK(vbe_window(&m, 65000, 640, &step, &in) == 640 && step == 15 && in == 3560);
    CHECK(vbe_window(&m, 65536 + 4000, 64000, &step, &in) == 61536 && step == 16);
    CHECK(in == 4000);
}
