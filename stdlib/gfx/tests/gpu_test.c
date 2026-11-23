/* The GPU's list, on a host that records what it is given to play and reads back pixels it is
   told: each frame's ops in order, its batches and vertices, worked by hand. */
#include "disc.h"
#include "gpu/list.h"
#include "soft/scan.h"
#include "surface.h"
#include "test.h"

enum { MOST_WORDS = 1024, MOST_VERTICES = 64, MOST_PIXELS = 64 };

/* The host: the last list played, its vertices and discs, the plays counted; the pixels a read
   gives. */
static QxHost fake;
static uint32_t words[MOST_WORDS], nwords, plays;
static GpuVertex vertices[MOST_VERTICES];
static GpuDisc discs[MOST_VERTICES];
static float uniforms[4]; /* the last UNIFORMS' */
static uint8_t pixels[MOST_PIXELS * 4];
static QxShader language; /* the one the host takes */
static bool down;         /* its clip space's y runs down */

static bool QX_CALL fake_open(QxScreen *const s)
{
    s->gpu = true;
    s->shader = language;
    s->y_down = down;
    s->out_w = s->w;
    s->out_h = s->h;
    return true;
}

static void QX_CALL fake_close(void) {}

static const void *pointer(const uint32_t *const a)
{
    return (const void *)(uintptr_t)((uint64_t)a[1] << 32 | a[0]);
}

/* A WRITE's bytes, of size an element, into out: MOST_VERTICES of them at most. */
static void copy(void *const out, const uint32_t *const a, const size_t size)
{
    const uint32_t n = a[1] / (uint32_t)size;

    if (n)
        memcpy(out, pointer(a + 2), (n < MOST_VERTICES ? n : MOST_VERTICES) * size);
}

/* The vertices and discs its WRITEs give, and its uniforms. */
static void QX_CALL fake_play(const uint32_t *const list, const uint32_t n)
{
    nwords = n < MOST_WORDS ? n : MOST_WORDS;
    memcpy(words, list, nwords * sizeof *words);

    for (uint32_t at = 0; at < n && list[at] >> QX_LIST_SIZE; at += list[at] >> QX_LIST_SIZE)
        if ((list[at] & 0xFF) == QX_LIST_WRITE && list[at + 1] == LIST_VERTICES)
            copy(vertices, list + at + 1, sizeof *vertices);
        else if ((list[at] & 0xFF) == QX_LIST_WRITE && list[at + 1] == LIST_DISCS)
            copy(discs, list + at + 1, sizeof *discs);
        else if ((list[at] & 0xFF) == QX_LIST_UNIFORMS && list[at + 1] == sizeof uniforms)
            memcpy(uniforms, pointer(list + at + 2), sizeof uniforms);

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

/* A w by h screen of depth on a fake host taking shader, y down or not, the state fresh. */
static void screen_in(
    Session *const S,
    const int32_t w,
    const int32_t h,
    const int32_t depth,
    const QxShader shader,
    const bool y_down
)
{
    language = shader;
    down = y_down;
    fake = S->natv.host;
    fake.minor = 2;
    fake.gfx_open = fake_open;
    fake.gfx_close = fake_close;
    fake.gpu_play = fake_play;
    fake.gpu_read = fake_read;
    qx_init_gfx(&fake)->stop();
    nwords = plays = 0;
    memset(pixels, 0, sizeof pixels);
    CHECK(!GPU_CALL("display.open", I(w), I(h), I(depth), I(0)).error);
    CHECK(GPU_CALL("display.caps").result.i == GFX_DISPLAY_HAS_GPU);
}

/* A w by h screen of depth on a fake Metal. */
static void screen(Session *const S, const int32_t w, const int32_t h, const int32_t depth)
{
    screen_in(S, w, h, depth, QX_SHADER_MSL, false);
}

/* A list's buffers' ops; the first's after them, the canvas, the palette and the pipelines. */
#define HEAD "BWBWBW"
#define FIRST HEAD "TTLLLLLL"

/* The last list's ops, a letter each: End, Buffer, Write, Texture, Free, Upload, pipeLine, Pass,
   Clip, uNiforms, Draw, Show. */
static bool ops(const char *const want)
{
    static const char letters[] = "EBWTFULPCNDS";
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

static bool placed(const GpuVertex *const v, const float x, const float y)
{
    return v->x == x && v->y == y;
}

/* The rows drawn, and the disc of the triangle's quad, or null. */
typedef struct {
    char rows[8][9];
    const GpuDisc *d;
} Cover;

/* The shader keeps x, y of d's quad: with DISC, where disc.h's rule covers it. */
static bool kept(const GpuDisc *const d, const int32_t x, const int32_t y)
{
    if (!d || !(d->v.stages & GPU_STAGE_DISC))
        return true;

    const Disc k = {{d->disc[0], d->disc[1]}, d->disc[2], d->disc[3]};

    const Sweep w = {
        {d->sweep[0], d->sweep[1]},
        {d->sweep[2], d->sweep[3]},
        d->v.stages & GPU_STAGE_WIDE,
        !(d->v.stages & GPU_STAGE_SWEEP),
    };

    const float dx = (float)x - k.c.x, dy = (float)y - k.c.y;

    return disc_covers(&k, dx, dy) && swept(&w, dx, dy);
}

/* The pixels of the last list's triangles, as a GPU covers them (centres inside, top and left
   edges in) and its shader keeps: '#' a pixel covered once, '2' more. */
static void cover(void *const ctx, const int32_t y, const int32_t x0, const int32_t x1)
{
    Cover *const c = ctx;

    for (int32_t x = x0; x < x1; x++)
        if (kept(c->d, x, y))
            c->rows[y][x] = c->rows[y][x] == '.' ? '#' : '2';
}

/* The last list's k-th draw covers want's pixels: its quads, four vertices to six indices. */
static bool covered(const uint32_t k, const char *const want[8])
{
    static const uint8_t triangles[2][3] = {{0, 1, 2}, {0, 2, 3}};

    const bool round = arg(QX_LIST_DRAW, k, 2) == LIST_DISCS;
    const uint32_t first = arg(QX_LIST_DRAW, k, 4) / 6 * 4, n = arg(QX_LIST_DRAW, k, 5) / 6 * 4;

    Cover c;
    memset(c.rows, '.', sizeof c.rows);

    for (uint32_t i = 0; i < 8; i++)
        c.rows[i][8] = 0;

    for (uint32_t q = first; q + 3 < first + n; q += 4) {
        c.d = round ? &discs[q] : nullptr;

        for (uint32_t k = 0; k < 2; k++) {
            Vec p[3];

            for (uint32_t i = 0; i < 3; i++) {
                const GpuVertex *const v =
                    round ? &discs[q + triangles[k][i]].v : &vertices[q + triangles[k][i]];
                p[i] = (Vec){v->x, v->y};
            }

            scan_convex(p, 3, (Rect){0, 0, 8, 8}, cover, &c);
        }
    }

    for (uint32_t i = 0; i < 8; i++)
        if (strcmp(c.rows[i], want[i]))
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
    CHECK(covered(0, line));

    screen(S, 8, 8, 32);
    GPU_CALL("draw.poly", ARRAY(&a), I(0), I(3), I(0xFFFFFF), I(0));
    GPU_CALL("display.flip");
    CHECK(covered(0, outline));
}

/* A circle and an arc are a box each, the shader keeping software's pixels (shape_test's): once
   polygons, a segment every two pixels, each a draw of its own. */
TEST(gfx_gpu_draws_a_circle_as_one_box)
{
    static const char *const disc[8] = {
        "........",
        "..###...",
        ".#####..",
        ".#####..",
        ".#####..",
        "..###...",
        "........",
        "........",
    };

    static const char *const arc[8] = {
        "........",
        "........",
        "........",
        ".....#..",
        ".....#..",
        "...##...",
        "........",
        "........",
    };

    screen(S, 8, 8, 32);
    GPU_CALL("draw.circle", F(3), F(3), F(2), I(0xFFFFFF), I(GFX_DRAW_FILL));
    GPU_CALL("display.flip");
    CHECK(ops(FIRST "TUPNCUDSE") && arg(QX_LIST_DRAW, 0, 2) == LIST_DISCS);
    CHECK(arg(QX_LIST_DRAW, 0, 5) == 6 && discs[0].v.stages == GPU_STAGE_DISC);
    CHECK(discs[0].disc[2] == 2.5f && discs[0].disc[3] == 0);
    CHECK(covered(0, disc));

    screen(S, 8, 8, 32);
    GPU_CALL("draw.arc", F(3), F(3), F(2), F(0), F(GM_PI / 2), I(0xFFFFFF));
    GPU_CALL("display.flip");
    CHECK(arg(QX_LIST_DRAW, 0, 5) == 6);
    CHECK(discs[0].v.stages == (GPU_STAGE_DISC | GPU_STAGE_SWEEP));
    CHECK(covered(0, arc));
}

/* A clear and a rect are one batch from the white texel, its colour in the vertices; a pass's
   uniforms its size and clip space's way down, Metal's up. */
TEST(gfx_gpu_draws_shapes_in_one_batch)
{
    screen(S, 8, 8, 32);
    GPU_CALL("draw.clear", I(0x102030));
    GPU_CALL("draw.rect", F(1), F(1), F(2), F(2), I(0xFF0000), I(GFX_DRAW_FILL));
    CHECK(!plays);
    GPU_CALL("display.flip");
    CHECK(plays == 1 && ops(FIRST "TUPNCUDSE"));
    CHECK(arg(QX_LIST_TEXTURE, 0, 1) == GFX_SURFACE_CANVAS && arg(QX_LIST_TEXTURE, 0, 2) == 8);
    CHECK(arg(QX_LIST_TEXTURE, 2, 1) == PIC_WHITE && arg(QX_LIST_TEXTURE, 2, 4) == QX_TEXTURE_RGBA);
    CHECK(arg(QX_LIST_PIPELINE, 0, 3) == sizeof(GpuVertex));
    CHECK(arg(QX_LIST_PIPELINE, 5, 3) == sizeof(GpuDisc));
    CHECK(arg(QX_LIST_PASS, 0, 1) == GFX_SURFACE_CANVAS && arg(QX_LIST_CLIP, 0, 3) == 8);
    CHECK(uniforms[0] == 8 && uniforms[1] == 8 && uniforms[2] == -1);
    CHECK(arg(QX_LIST_DRAW, 0, 4) == 0 && arg(QX_LIST_DRAW, 0, 5) == 12);
    CHECK(arg(QX_LIST_DRAW, 0, 7) == PIC_WHITE && arg(QX_LIST_DRAW, 0, 1) == 1);
    CHECK(placed(&vertices[0], -0.5f, -0.5f) && placed(&vertices[2], 7.5f, 7.5f));
    CHECK(vertices[0].r == 0x10 && vertices[0].g == 0x20 && vertices[0].b == 0x30);
    CHECK(
        placed(&vertices[4], 0.5f, 0.5f) && placed(&vertices[6], 2.5f, 2.5f) &&
        vertices[4].r == 0xFF
    );
    CHECK(vertices[4].u == 0.5f && vertices[4].rw == 1 && vertices[4].light == 63);
}

/* A WebGL2 host gets ESSL, its y down the rows: the shader is told 1. */
TEST(gfx_gpu_speaks_the_hosts_language)
{
    screen_in(S, 8, 8, 32, QX_SHADER_ESSL, true);
    GPU_CALL("draw.clear", I(0));
    GPU_CALL("display.flip");

    for (uint32_t k = 0; k < 6; k++) {
        const uint32_t vertex[] = {arg(QX_LIST_PIPELINE, k, 6), arg(QX_LIST_PIPELINE, k, 7)};

        const uint32_t fragment[] = {arg(QX_LIST_PIPELINE, k, 9), arg(QX_LIST_PIPELINE, k, 10)};

        CHECK(!memcmp(pointer(vertex), "#version 300 es", 15));
        CHECK(!memcmp(pointer(fragment), "#version 300 es", 15));
    }

    CHECK(uniforms[2] == 1);
}

/* BLEND at alpha 32 is its own batch, alpha 32 of 63 as 129 of 255. */
TEST(gfx_gpu_splits_batches_at_a_blend)
{
    screen(S, 8, 8, 32);
    GPU_CALL("draw.rect", F(0), F(0), F(1), F(1), I(1), I(GFX_DRAW_FILL));
    GPU_CALL("pen.alpha", I(32));
    GPU_CALL("draw.rect", F(0), F(0), F(1), F(1), I(1), I(GFX_DRAW_FILL | GFX_PEN_BLEND));
    GPU_CALL("display.flip");
    CHECK(ops(FIRST "TUPNCUDDSE"));
    CHECK(arg(QX_LIST_DRAW, 1, 1) == 1 + QX_BLEND_MIX && arg(QX_LIST_DRAW, 1, 6) == 129);
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
    CHECK(ops(FIRST "TUPNCUTUDSE"));
    CHECK(
        arg(QX_LIST_TEXTURE, 3, 1) == (uint32_t)s && arg(QX_LIST_TEXTURE, 3, 4) == QX_TEXTURE_INDEX
    );
    CHECK(arg(QX_LIST_DRAW, 0, 7) == (uint32_t)s);
    CHECK(placed(&vertices[0], 0.5f, 0.5f) && vertices[0].u == 2 && vertices[1].u == 0);
    CHECK(vertices[0].stages == (GPU_STAGE_INDEXED | GPU_STAGE_KEYED) && vertices[0].rw == 2);

    GPU_CALL("draw.sprite", I(s), F(1), F(1), I(0));
    GPU_CALL("display.flip");
    CHECK(ops(HEAD "PNCDSE"));

    GPU_CALL("surface.write", I(s), I(0), I(0), I(2), I(2), ARRAY(&a));
    GPU_CALL("draw.sprite", I(s), F(1), F(1), I(0));
    GPU_CALL("display.flip");
    CHECK(ops(HEAD "PNCUDSE"));

    GPU_CALL("surface.free", I(s));
    GPU_CALL("display.flip");
    CHECK(ops(HEAD "FSE") && arg(QX_LIST_FREE, 0, 1) == (uint32_t)s);
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
    CHECK(ops(FIRST "TUPNCUTDFTUPNCDPNCDSE"));
    CHECK(arg(QX_LIST_TEXTURE, 4, 4) == QX_TEXTURE_RGBA && arg(QX_LIST_PASS, 1, 1) == (uint32_t)s);
    CHECK(arg(QX_LIST_CLIP, 1, 3) == 2 && vertices[8].stages == GPU_STAGE_KEYED);
    CHECK(vertices[4].r == 0xAA && vertices[4].g == 0 && vertices[4].b == 0); /* index 4 */
}

/* A read plays what was drawn and reads it back: 32 bits as they are; 8 bits the nearest index,
   alpha under a half index 0. */
TEST(gfx_gpu_reads_back)
{
    screen(S, 4, 1, 32);
    GPU_CALL("draw.pset", F(0), F(0), I(0x123456));
    memcpy(pixels, (const uint8_t[]){0x12, 0x34, 0x56, 0xFF}, 4);

    CHECK(GPU_CALL("surface.point", I(GFX_SURFACE_CANVAS), I(0), I(0)).result.i == 0x123456);
    CHECK(plays == 1 && ops(FIRST "TUPNCUDE"));
    CHECK(GPU_CALL("surface.point", I(GFX_SURFACE_CANVAS), I(1), I(0)).result.i == 0);
    CHECK(plays == 1);

    screen(S, 4, 1, 8);
    GPU_CALL("draw.pset", F(0), F(0), I(4));
    memcpy(pixels, (const uint8_t[]){0xAA, 0, 0, 0xFF, 0xAA, 0, 0, 0x10}, 8);

    CHECK(GPU_CALL("surface.point", I(GFX_SURFACE_CANVAS), I(0), I(0)).result.i == 4);
    CHECK(GPU_CALL("surface.point", I(GFX_SURFACE_CANVAS), I(1), I(0)).result.i == 0);
}
