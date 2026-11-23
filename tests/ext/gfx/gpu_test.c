/* The GPU's list, on a host that records what it is given to play and reads back pixels it is
   told: each frame's ops in order, its batches and vertices, worked by hand. */
#include "qxlist.h"
#include "soft/scan.h"
#include "surface.h"
#include "test.h"

enum { MOST_WORDS = 1024, MOST_VERTICES = 64, MOST_PIXELS = 64 };

/* The host: the last list played, its vertices, the plays counted; the pixels a read gives. */
static QxHost fake;
static uint32_t words[MOST_WORDS], nwords, plays;
static QxVertex vertices[MOST_VERTICES];
static uint8_t pixels[MOST_PIXELS * 4];

static bool QX_CALL fake_open(QxScreen *const s)
{
    s->gpu = true;
    s->out_w = s->w;
    s->out_h = s->h;
    return true;
}

static void QX_CALL fake_close(void) {}

static void QX_CALL fake_play(const uint32_t *const list, const uint32_t n)
{
    const QxVertex *const v = (const QxVertex *)(uintptr_t)((uint64_t)list[3] << 32 | list[2]);

    nwords = n < MOST_WORDS ? n : MOST_WORDS;
    memcpy(words, list, nwords * sizeof *words);
    memcpy(vertices, v, (list[1] < MOST_VERTICES ? list[1] : MOST_VERTICES) * sizeof *v);
    plays++;
}

static void QX_CALL fake_read(
    const uint32_t texture,
    const int32_t x,
    const int32_t y,
    const int32_t w,
    const int32_t h,
    uint8_t *const out
)
{
    (void)texture, (void)x, (void)y;
    memcpy(out, pixels, (size_t)(w * h * 4));
}

#define GPU_CALL(name, ...) CALL_ON(&fake, name __VA_OPT__(, ) __VA_ARGS__)

/* A w by h screen of depth on the fake host, the state fresh. */
static void screen(Session *const S, const int32_t w, const int32_t h, const int32_t depth)
{
    fake = S->natv.host;
    fake.minor = 2;
    fake.gfx_open = fake_open;
    fake.gfx_close = fake_close;
    fake.gfx_play = fake_play;
    fake.gfx_read = fake_read;
    qx_init_gfx(&fake)->stop();
    nwords = plays = 0;
    memset(pixels, 0, sizeof pixels);
    CHECK(!GPU_CALL("display.open", I(w), I(h), I(depth), I(0)).error);
    CHECK(GPU_CALL("display.caps").result.i == GFX_DISPLAY_HAS_GPU);
}

/* The last list's ops, a letter each: Vertices, Texture, Free, Upload, paLette, Pass, Clip, Draw,
   Show, End. */
static bool ops(const char *const want)
{
    static const char letters[] = "EVTFULPCDS";
    char got[64];
    uint32_t n = 0;

    for (uint32_t at = 0; at < nwords && n < sizeof got; at += words[at] >> QX_LIST_SIZE) {
        const uint32_t op = words[at] & 0xFF;
        got[n++] = op < sizeof letters - 1 ? letters[op] : '?';

        if (!(words[at] >> QX_LIST_SIZE))
            break;
    }

    return n == strlen(want) && !memcmp(got, want, n);
}

/* The k-th op of code op's word i. */
static uint32_t arg(const QxListOp op, const uint32_t k, const uint32_t i)
{
    uint32_t seen = 0;

    for (uint32_t at = 0; at < nwords && words[at] >> QX_LIST_SIZE; at += words[at] >> QX_LIST_SIZE)
        if ((words[at] & 0xFF) == op && seen++ == k)
            return words[at + i];

    return 0xDEAD;
}

static bool placed(const QxVertex *const v, const float x, const float y)
{
    return v->x == x && v->y == y;
}

/* The pixels of the last list's triangles, as a GPU covers them (centres inside, top and left
   edges in): '#' a pixel covered once, '2' more. */
static void cover(void *const ctx, const int32_t y, const int32_t x0, const int32_t x1)
{
    char (*const rows)[9] = ctx;

    for (int32_t x = x0; x < x1; x++)
        rows[y][x] = rows[y][x] == '.' ? '#' : '2';
}

static bool covered(const uint32_t first, const uint32_t n, const char *const want[8])
{
    char rows[8][9];
    memset(rows, '.', sizeof rows);

    for (uint32_t i = 0; i < 8; i++)
        rows[i][8] = 0;

    for (uint32_t i = first; i + 2 < first + n; i += 3) {
        const Vec p[] = {
            {vertices[i].x, vertices[i].y},
            {vertices[i + 1].x, vertices[i + 1].y},
            {vertices[i + 2].x, vertices[i + 2].y},
        };

        scan_convex(p, 3, (Rect){0, 0, 8, 8}, cover, rows);
    }

    for (uint32_t i = 0; i < 8; i++)
        if (strcmp(rows[i], want[i]))
            return false;

    return true;
}

/* A line covers software's pixels, one a step along its longer side: a quad a pixel wide across
   it once covered two at each step of a diagonal. An outline's edge leaves its end to the next,
   each corner covered once: the quads once overlapped there, drawn twice where blended. */
TEST(gfx_gpu_lines_cover_softwares_pixels)
{
    static const char *const line[8] = {
        "........",
        "........",
        ".......#",
        ".....##.",
        "....#...",
        "...#....",
        ".##.....",
        "#.......",
    };

    static const char *const outline[8] = {
        "........",
        ".######.",
        ".#...#..",
        ".#..#...",
        ".#.#....",
        ".##.....",
        ".#......",
        "........",
    };

    gfx_draw_vec2 tri[] = {{1, 1}, {6, 1}, {1, 6}};

    QxArray a = gfx_array(tri, 3, sizeof *tri);

    screen(S, 8, 8, 32);
    GPU_CALL("draw.line", F(0), F(7), F(7), F(2), I(0xFFFFFF));
    GPU_CALL("display.flip");
    CHECK(covered(0, arg(QX_LIST_DRAW, 0, 2), line));

    screen(S, 8, 8, 32);
    GPU_CALL("draw.poly", ARRAY(&a), I(0), I(3), I(0xFFFFFF), I(0));
    GPU_CALL("display.flip");
    CHECK(covered(0, arg(QX_LIST_DRAW, 0, 2), outline));
}

/* A clear and a rect are one batch from the white texel, its colour in the vertices. */
TEST(gfx_gpu_draws_shapes_in_one_batch)
{
    screen(S, 8, 8, 32);
    GPU_CALL("draw.clear", I(0x102030));
    GPU_CALL("draw.rect", F(1), F(1), F(2), F(2), I(0xFF0000), I(GFX_DRAW_FILL));
    CHECK(!plays);
    GPU_CALL("display.flip");
    CHECK(plays == 1 && ops("VTUPCLDSE"));
    CHECK(arg(QX_LIST_TEXTURE, 0, 1) == PIC_WHITE && arg(QX_LIST_TEXTURE, 0, 4) == QX_TEXTURE_RGBA);
    CHECK(arg(QX_LIST_PASS, 0, 1) == QX_CANVAS && arg(QX_LIST_CLIP, 0, 3) == 8);
    CHECK(arg(QX_LIST_DRAW, 0, 1) == 0 && arg(QX_LIST_DRAW, 0, 2) == 12);
    CHECK(arg(QX_LIST_DRAW, 0, 3) == PIC_WHITE && arg(QX_LIST_DRAW, 0, 4) == QX_BLEND_NONE);
    CHECK(placed(&vertices[0], -0.5f, -0.5f) && placed(&vertices[2], 7.5f, 7.5f));
    CHECK(vertices[0].r == 0x10 && vertices[0].g == 0x20 && vertices[0].b == 0x30);
    CHECK(
        placed(&vertices[6], 0.5f, 0.5f) && placed(&vertices[8], 2.5f, 2.5f) &&
        vertices[6].r == 0xFF
    );
    CHECK(vertices[6].u == 0.5f && vertices[6].rw == 1 && vertices[6].light == 63);
}

/* BLEND at alpha 32 is its own batch, alpha 32 of 63 as 129 of 255. */
TEST(gfx_gpu_splits_batches_at_a_blend)
{
    screen(S, 8, 8, 32);
    GPU_CALL("draw.rect", F(0), F(0), F(1), F(1), I(1), I(GFX_DRAW_FILL));
    GPU_CALL("pen.alpha", I(32));
    GPU_CALL("draw.rect", F(0), F(0), F(1), F(1), I(1), I(GFX_DRAW_FILL | GFX_PEN_BLEND));
    GPU_CALL("display.flip");
    CHECK(ops("VTUPCLDDSE"));
    CHECK(arg(QX_LIST_DRAW, 1, 4) == QX_BLEND_MIX && arg(QX_LIST_DRAW, 1, 5) == 129);
}

/* An 8-bit picture is an index texture, sent once and again when written; freed with it. */
TEST(gfx_gpu_sends_a_picture_once)
{
    screen(S, 8, 8, 8);
    const int32_t s = GPU_CALL("surface.new", I(2), I(2)).result.i;
    uint8_t texels[] = {0, 5, 6, 7};

    QxArray a = gfx_array(texels, 4, 1);
    GPU_CALL("surface.write", I(s), I(0), I(0), I(2), I(2), ARRAY(&a));

    GPU_CALL("draw.sprite", I(s), F(1), F(1), I(GFX_DRAW_FLIPX));
    GPU_CALL("display.flip");
    CHECK(ops("VTUPCLTUDSE"));
    CHECK(
        arg(QX_LIST_TEXTURE, 1, 1) == (uint32_t)s && arg(QX_LIST_TEXTURE, 1, 4) == QX_TEXTURE_INDEX
    );
    CHECK(arg(QX_LIST_DRAW, 0, 3) == (uint32_t)s);
    CHECK(placed(&vertices[0], 0.5f, 0.5f) && vertices[0].u == 2 && vertices[1].u == 0);
    CHECK(vertices[0].stages == (QX_STAGE_INDEXED | QX_STAGE_KEYED) && vertices[0].rw == 2);

    GPU_CALL("draw.sprite", I(s), F(1), F(1), I(0));
    GPU_CALL("display.flip");
    CHECK(ops("VPCDSE"));

    GPU_CALL("surface.write", I(s), I(0), I(0), I(2), I(2), ARRAY(&a));
    GPU_CALL("draw.sprite", I(s), F(1), F(1), I(0));
    GPU_CALL("display.flip");
    CHECK(ops("VPCUDSE"));

    GPU_CALL("surface.free", I(s));
    GPU_CALL("display.flip");
    CHECK(ops("VFSE") && arg(QX_LIST_FREE, 0, 1) == (uint32_t)s);
}

/* A surface drawn into is an RGBA target, drawn from as it is; an 8-bit one drawn from first is
   made again as RGBA, its texels sent through row 0. */
TEST(gfx_gpu_draws_into_surfaces)
{
    screen(S, 8, 8, 8);
    const int32_t s = GPU_CALL("surface.new", I(2), I(2)).result.i;
    GPU_CALL("draw.sprite", I(s), F(0), F(0), I(0));
    GPU_CALL("pen.target", I(s));
    GPU_CALL("draw.clear", I(4));
    GPU_CALL("pen.target", I(GFX_SURFACE_CANVAS));
    GPU_CALL("draw.sprite", I(s), F(0), F(0), I(0));
    GPU_CALL("display.flip");
    CHECK(ops("VTUPCLTDFTUPCDPCDSE"));
    CHECK(arg(QX_LIST_TEXTURE, 2, 4) == QX_TEXTURE_RGBA && arg(QX_LIST_PASS, 1, 1) == (uint32_t)s);
    CHECK(arg(QX_LIST_CLIP, 1, 3) == 2 && vertices[12].stages == QX_STAGE_KEYED);
    CHECK(vertices[6].r == 0xAA && vertices[6].g == 0 && vertices[6].b == 0); /* index 4 */
}

/* A read plays what was drawn and reads it back: 32 bits as they are; 8 bits the nearest index,
   alpha under a half index 0. */
TEST(gfx_gpu_reads_back)
{
    screen(S, 4, 1, 32);
    GPU_CALL("draw.pset", F(0), F(0), I(0x123456));
    memcpy(pixels, (const uint8_t[]){0x12, 0x34, 0x56, 0xFF}, 4);

    CHECK(GPU_CALL("surface.point", I(GFX_SURFACE_CANVAS), I(0), I(0)).result.i == 0x123456);
    CHECK(plays == 1 && ops("VTUPCLDE"));
    CHECK(GPU_CALL("surface.point", I(GFX_SURFACE_CANVAS), I(1), I(0)).result.i == 0);
    CHECK(plays == 1);

    screen(S, 4, 1, 8);
    GPU_CALL("draw.pset", F(0), F(0), I(4));
    memcpy(pixels, (const uint8_t[]){0xAA, 0, 0, 0xFF, 0xAA, 0, 0, 0x10}, 8);

    CHECK(GPU_CALL("surface.point", I(GFX_SURFACE_CANVAS), I(0), I(0)).result.i == 4);
    CHECK(GPU_CALL("surface.point", I(GFX_SURFACE_CANVAS), I(1), I(0)).result.i == 0);
}
