/* The GPU's list, on a host that records what it is given to play and reads back pixels it is
   told: each frame's ops in order, its batches and vertices, worked by hand. */
#include <stdio.h>
#include "base/disc.h"
#include "backend/gpu/gpu.h"
#include "backend/gpu/pipelines.h"
#include "base/cover.h"
#include "core/surface.h"
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
    fake.minor = 4;
    fake.gfx_open = fake_open;
    fake.gfx_close = fake_close;
    fake.gpu_play = fake_play;
    fake.gpu_read = fake_read;
    qx_init_gfx(&fake)->stop();
    nwords = plays = 0;
    memset(pixels, 0, sizeof pixels);
    CHECK(!GPU_CALL("display.open", I(w), I(h), I(depth), I(0)).error);
    CHECK(GPU_CALL("display.features").result.i == (GFX_DISPLAY_HAS_GPU | GFX_DISPLAY_HAS_SMOOTH));
}

/* A w by h screen of depth on a fake Metal. */
static void screen(Session *const S, const int32_t w, const int32_t h, const int32_t depth)
{
    screen_in(S, w, h, depth, QX_SHADER_MSL, false);
}

/* A list's buffers' ops; the first's after them, the canvas and the palette. A pipeline's op
   comes before its first draw. */
#define HEAD "BWBWBW"
#define FIRST HEAD "TT"

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

    const bool same = n == strlen(want) && !memcmp(got, want, n);

    if (!same)
        fprintf(stderr, "ops: %.*s, not %s\n", (int)n, got, want);

    return same;
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

typedef void ScanSpan(void *ctx, int32_t y, int32_t x0, int32_t x1);

/* A GPU's rule, kept here as the oracle: a convex polygon's pixels whose centres are inside, a
   top or left edge's in, clipped, one span a row. */
static void scan_convex(
    const Vec *const p,
    const uint32_t n,
    const Rect clip,
    ScanSpan *const span,
    void *const ctx
)
{
    float top = p[0].y, bottom = p[0].y;

    for (uint32_t i = 1; i < n; i++) {
        top = p[i].y < top ? p[i].y : top;
        bottom = p[i].y > bottom ? p[i].y : bottom;
    }

    const int32_t y0 = rect_max(cover_first(top), clip.y);
    const int32_t y1 = rect_min(cover_first(bottom), clip.y + clip.h);

    for (int32_t y = y0; y < y1; y++) {
        float left = 0, right = 0;
        bool crossed = false;

        for (uint32_t i = 0; i < n; i++) {
            const Vec e = p[i], f = p[(i + 1) % n];

            if ((e.y <= (float)y) == (f.y <= (float)y))
                continue;

            const Vec a = e.y < f.y ? e : f, b = e.y < f.y ? f : e;
            const float x = a.x + ((float)y - a.y) * (b.x - a.x) / (b.y - a.y);
            left = !crossed || x < left ? x : left;
            right = !crossed || x > right ? x : right;
            crossed = true;
        }

        const int32_t x0 = rect_max(cover_first(left), clip.x);
        const int32_t x1 = rect_min(cover_first(right), clip.x + clip.w);

        if (crossed && x0 < x1)
            span(ctx, y, x0, x1);
    }
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

    gfx_draw_vertex tri[] = {{1, 1, 1, 0, 0}, {6, 1, 1, 0, 0}, {1, 6, 1, 0, 0}};

    QxArray a = gfx_array(tri, 3, sizeof *tri);

    screen(S, 8, 8, 32);
    GPU_CALL("draw.line", I(SURF_SCREEN), F(0), F(7), F(7), F(2), I(0xFFFFFF));
    GPU_CALL("display.flip");
    CHECK(covered(0, line));

    screen(S, 8, 8, 32);
    GPU_CALL("draw.poly", I(SURF_SCREEN), ARRAY(&a), I(0), I(3), I(0), I(0xFFFFFF), I(0));
    GPU_CALL("display.flip");
    CHECK(covered(0, outline));
}

enum { RULE_W = 56, RULE_H = 48 };

static void rule_mark(void *const ctx, const int32_t y, const int32_t x0, const int32_t x1)
{
    bool (*const in)[RULE_W] = ctx;

    for (int32_t x = x0; x < x1; x++)
        in[y][x] = true;
}

/* Software's filled polygons cover the GPU's pixels, ties on a centre included: bench's
   triangle at whole pixels, and moved by fractions. The scanner once took a unit's nudge up as
   its rows' y: an edge steeper than a pixel a row took the centres on it, 8 a triangle. */
TEST(gfx_soft_polys_cover_a_gpus_pixels)
{
    static const float at[][2] = {{3, 2}, {3.5f, 2.5f}, {3.25f, 2.75f}, {4.7f, 1.1f}};

    for (uint32_t k = 0; k < sizeof at / sizeof *at; k++) {
        const float x = at[k][0], y = at[k][1];
        gfx_draw_vertex tri[] =
            {{x, y, 1, 0, 0}, {x + 40, y + 8, 1, 0, 0}, {x + 12, y + 40, 1, 0, 0}};

        const Vec p[] = {{x, y}, {x + 40, y + 8}, {x + 12, y + 40}};

        QxArray a = gfx_array(tri, 3, sizeof *tri);
        bool in[RULE_H][RULE_W] = {};

        CANVAS(RULE_W, RULE_H);
        CHECK(
            !CALL("draw.poly", I(SURF_SCREEN), ARRAY(&a), I(0), I(3), I(0), I(2), I(GFX_DRAW_FILL))
                 .error
        );
        scan_convex(p, 3, (Rect){0, 0, RULE_W, RULE_H}, rule_mark, in);

        int32_t wrong = 0;

        for (int32_t j = 0; j < RULE_H; j++)
            for (int32_t i = 0; i < RULE_W; i++)
                wrong += (PX(i, j) == 2) != in[j][i];

        CHECK(wrong == 0);
    }
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
    GPU_CALL("draw.circle", I(SURF_SCREEN), F(3), F(3), F(2), I(0xFFFFFF), I(GFX_DRAW_FILL));
    GPU_CALL("display.flip");
    CHECK(ops(FIRST "TUPNCULDSE") && arg(QX_LIST_DRAW, 0, 2) == LIST_DISCS);
    CHECK(arg(QX_LIST_PIPELINE, 0, 3) == sizeof(GpuDisc));
    CHECK(arg(QX_LIST_DRAW, 0, 5) == 6 && discs[0].v.stages == (GPU_STAGE_SOLID | GPU_STAGE_DISC));
    CHECK(discs[0].disc[2] == 2.5f && discs[0].disc[3] == 0);
    CHECK(covered(0, disc));

    screen(S, 8, 8, 32);
    GPU_CALL("draw.arc", I(SURF_SCREEN), F(3), F(3), F(2), F(0), F(GM_PI / 2), I(0xFFFFFF));
    GPU_CALL("display.flip");
    CHECK(arg(QX_LIST_DRAW, 0, 5) == 6);
    CHECK(discs[0].v.stages == (GPU_STAGE_SOLID | GPU_STAGE_DISC | GPU_STAGE_SWEEP));
    CHECK(covered(0, arc));
}

/* A clear and a rect are one batch from the white texel, its colour in the vertices; a pass's
   uniforms its size and clip space's way down, Metal's up. */
TEST(gfx_gpu_draws_shapes_in_one_batch)
{
    screen(S, 8, 8, 32);
    GPU_CALL("draw.clear", I(SURF_SCREEN), I(0x102030));
    GPU_CALL("draw.rect", I(SURF_SCREEN), F(1), F(1), F(2), F(2), I(0xFF0000), I(GFX_DRAW_FILL));
    CHECK(!plays);
    GPU_CALL("display.flip");
    CHECK(plays == 1 && ops(FIRST "TUPNCULDSE"));
    CHECK(arg(QX_LIST_TEXTURE, 0, 1) == SURF_SCREEN && arg(QX_LIST_TEXTURE, 0, 2) == 8);
    CHECK(
        arg(QX_LIST_TEXTURE, 2, 1) == GPU_PAGE_FIRST &&
        arg(QX_LIST_TEXTURE, 2, 4) == QX_TEXTURE_RGBA
    );
    CHECK(arg(QX_LIST_TEXTURE, 2, 2) == GPU_PAGE && arg(QX_LIST_UPLOAD, 0, 1) == GPU_PAGE_FIRST);
    CHECK(arg(QX_LIST_PIPELINE, 0, 3) == sizeof(GpuVertex));
    CHECK(arg(QX_LIST_PASS, 0, 1) == SURF_SCREEN && arg(QX_LIST_CLIP, 0, 3) == 8);
    CHECK(uniforms[0] == 8 && uniforms[1] == 8 && uniforms[2] == -1);
    CHECK(arg(QX_LIST_DRAW, 0, 4) == 0 && arg(QX_LIST_DRAW, 0, 5) == 12);
    CHECK(arg(QX_LIST_DRAW, 0, 7) == GPU_PAGE_FIRST);
    CHECK(arg(QX_LIST_DRAW, 0, 1) == pipeline_id(PIPELINE_SIMPLE, QX_BLEND_NONE, 0));
    CHECK(placed(&vertices[0], -0.5f, -0.5f) && placed(&vertices[2], 7.5f, 7.5f));
    CHECK(vertices[0].r == 0x10 && vertices[0].g == 0x20 && vertices[0].b == 0x30);
    CHECK(
        placed(&vertices[4], 0.5f, 0.5f) && placed(&vertices[6], 2.5f, 2.5f) &&
        vertices[4].r == 0xFF
    );
    CHECK(vertices[4].u == 0.5f && vertices[4].rw == 1 && vertices[4].light == 63);
}

/* stats: the frame shown last's draws, its batches, its uploads (the white texel, the palette) and
   the reads that waited for the GPU, which count towards the next. */
TEST(gfx_gpu_counts_the_frame)
{
    gfx_display_framestats s = {};

    screen(S, 8, 8, 32);
    GPU_CALL("draw.clear", I(SURF_SCREEN), I(0));
    GPU_CALL("draw.rect", I(SURF_SCREEN), F(1), F(1), F(2), F(2), I(0xFF0000), I(GFX_DRAW_FILL));
    GPU_CALL("display.flip");
    GPU_CALL("display.get_stats", REF(&s));
    CHECK(s.draws == 2 && s.batches == 1 && s.uploads == 2 && s.waits == 0);

    GPU_CALL("surface.point", I(SURF_SCREEN), I(1), I(1));
    GPU_CALL("display.flip");
    GPU_CALL("display.get_stats", REF(&s));
    CHECK(s.draws == 0 && s.batches == 0 && s.uploads == 0 && s.waits == 1);
}

/* A WebGL2 host gets ESSL, its y down the rows: the shader is told 1. */
TEST(gfx_gpu_speaks_the_hosts_language)
{
    screen_in(S, 8, 8, 32, QX_SHADER_ESSL, true);
    GPU_CALL("draw.clear", I(SURF_SCREEN), I(0));
    GPU_CALL("display.flip");

    const uint32_t vertex[] = {arg(QX_LIST_PIPELINE, 0, 6), arg(QX_LIST_PIPELINE, 0, 7)};

    const uint32_t fragment[] = {arg(QX_LIST_PIPELINE, 0, 9), arg(QX_LIST_PIPELINE, 0, 10)};

    CHECK(ops(FIRST "TUPNCULDSE"));
    CHECK(!memcmp(pointer(vertex), "#version 300 es", 15));
    CHECK(!memcmp(pointer(fragment), "#version 300 es", 15));

    CHECK(uniforms[2] == 1);
}

/* BLEND at 32 bits is the pipeline's mix, alpha 32 of 63 as 130 of 255, overlapping blends in one
   batch. It read a copy of behind, made again before each blend over the last: a pass a copy. */
TEST(gfx_gpu_blends_32_bits_in_the_pipeline)
{
    const int32_t blend = GFX_DRAW_FILL | GFX_DRAW_BLEND;

    screen(S, 8, 8, 32);
    GPU_CALL("draw.rect", I(SURF_SCREEN), F(0), F(0), F(1), F(1), I(1), I(GFX_DRAW_FILL));
    GPU_CALL("draw.set_alpha", I(32));
    GPU_CALL("draw.rect", I(SURF_SCREEN), F(0), F(0), F(1), F(1), I(1), I(blend));
    GPU_CALL("draw.rect", I(SURF_SCREEN), F(0), F(1), F(1), F(2), I(1), I(blend));
    GPU_CALL("display.flip");
    CHECK(ops(FIRST "TUPNCULDLDSE") && arg(QX_LIST_PIPELINE, 1, 2) == QX_BLEND_MIX);
    CHECK(arg(QX_LIST_DRAW, 1, 1) == arg(QX_LIST_PIPELINE, 1, 1) && arg(QX_LIST_DRAW, 1, 6) == 130);
    CHECK(arg(QX_LIST_DRAW, 1, 5) == 2 * 6 && vertices[4].stages == GPU_STAGE_SOLID);
}

/* At 15 and 16 bits a draw keeps a channel's top bits, as software's pixel; a blend, the pipeline's,
   is not narrowed after. The target kept all 8: every colour up to 7 off software's. */
TEST(gfx_gpu_narrows_15_and_16_bits)
{
    const int32_t fill = GFX_DRAW_FILL;

    screen(S, 8, 8, 16);
    GPU_CALL("draw.rect", I(SURF_SCREEN), F(0), F(0), F(1), F(1), I(1), I(fill));
    GPU_CALL("draw.rect", I(SURF_SCREEN), F(2), F(0), F(3), F(1), I(1), I(fill | GFX_DRAW_BLEND));
    GPU_CALL("display.flip");
    CHECK(vertices[0].stages == (GPU_STAGE_SOLID | GPU_STAGE_SIXTEEN));
    CHECK(vertices[4].stages == GPU_STAGE_SOLID);
    CHECK(arg(QX_LIST_DRAW, 0, 1) == pipeline_id(PIPELINE_SIMPLE, QX_BLEND_NONE, 0));

    screen(S, 8, 8, 15);
    GPU_CALL("draw.rect", I(SURF_SCREEN), F(0), F(0), F(1), F(1), I(1), I(fill));
    GPU_CALL("display.flip");
    CHECK(vertices[0].stages == (GPU_STAGE_SOLID | GPU_STAGE_FIFTEEN));
}

/* BLEND at 8 bits mixes in the shader with a copy of what is behind, made first: the rect before
   it covers its pixels. One beside it reads that copy, in its batch; one over it, a copy made
   again of the two drawn since alone. */
TEST(gfx_gpu_blends_8_bits_read_a_copy_of_behind)
{
    const int32_t blend = GFX_DRAW_FILL | GFX_DRAW_BLEND;
    const uint16_t mix = GPU_STAGE_SOLID | GPU_STAGE_MIX | GPU_STAGE_EIGHT;

    screen(S, 8, 8, 8);
    GPU_CALL("draw.rect", I(SURF_SCREEN), F(0), F(0), F(1), F(1), I(1), I(GFX_DRAW_FILL));
    GPU_CALL("draw.set_alpha", I(32));
    GPU_CALL("draw.rect", I(SURF_SCREEN), F(0), F(0), F(1), F(1), I(1), I(blend));
    GPU_CALL("draw.rect", I(SURF_SCREEN), F(4), F(4), F(5), F(5), I(1), I(blend));
    GPU_CALL("draw.rect", I(SURF_SCREEN), F(0), F(1), F(1), F(2), I(1), I(blend));
    GPU_CALL("display.flip");
    CHECK(
        ops(FIRST "TUPNCULD"
                  "UTPNCDPNCLD"
                  "PNCDPNCD"
                  "SE")
    );
    CHECK(arg(QX_LIST_DRAW, 1, 7) == SURF_SCREEN && arg(QX_LIST_DRAW, 2, 9) == GPU_BEHIND);
    CHECK(arg(QX_LIST_DRAW, 2, 5) == 2 * 6 && arg(QX_LIST_DRAW, 2, 6) == 255);
    CHECK(vertices[8].alpha == 32 && vertices[8].stages == mix);
    CHECK(arg(QX_LIST_DRAW, 3, 7) == SURF_SCREEN && arg(QX_LIST_DRAW, 3, 5) == 2 * 6);
}

/* A shape's texel does not matter: it joins a texture's batch, before or after it, and a circle
   one of circles whatever their texture. */
TEST(gfx_gpu_draws_shapes_and_textures_in_one_batch)
{
    screen(S, 8, 8, 8);
    const int32_t s = GPU_CALL("surface.new", I(2), I(2)).result.i;

    GPU_CALL("draw.sprite", I(SURF_SCREEN), F(0), F(0), I(s), I(0));
    GPU_CALL("display.flip");
    GPU_CALL("draw.rect", I(SURF_SCREEN), F(0), F(0), F(1), F(1), I(1), I(GFX_DRAW_FILL));
    GPU_CALL("draw.sprite", I(SURF_SCREEN), F(0), F(0), I(s), I(0));
    GPU_CALL("draw.line", I(SURF_SCREEN), F(0), F(0), F(5), F(2), I(2));
    GPU_CALL("draw.sprite", I(SURF_SCREEN), F(4), F(4), I(s), I(0));
    GPU_CALL("draw.circle", I(SURF_SCREEN), F(4), F(4), F(2), I(3), I(GFX_DRAW_FILL));
    GPU_CALL("draw.circle", I(SURF_SCREEN), F(4), F(4), F(3), I(3), I(0));
    GPU_CALL("display.flip");
    CHECK(ops(HEAD "PNCDLDSE"));
    CHECK(arg(QX_LIST_DRAW, 0, 7) == GPU_PAGE_FIRST + 1 && arg(QX_LIST_DRAW, 0, 5) == 4 * 6);
    CHECK(arg(QX_LIST_DRAW, 1, 2) == LIST_DISCS && arg(QX_LIST_DRAW, 1, 5) == 2 * 6);
    CHECK(vertices[0].stages == (GPU_STAGE_SOLID | GPU_STAGE_EIGHT));
    CHECK(vertices[4].stages == (GPU_STAGE_INDEXED | GPU_STAGE_KEYED | GPU_STAGE_EIGHT));
}

/* Textures drawn from share a page, side by side in a row, and so a batch once sent; one larger
   than half a page has a texture of its own. A page empty again starts over. */
TEST(gfx_gpu_packs_textures_into_pages)
{
    screen(S, 8, 8, 32);
    const int32_t a = GPU_CALL("surface.new", I(2), I(2)).result.i;
    const int32_t b = GPU_CALL("surface.new", I(3), I(1)).result.i;
    const int32_t big = GPU_CALL("surface.new", I(GPU_PAGE / 2 + 1), I(1)).result.i;

    for (uint32_t frame = 0; frame < 2; frame++) {
        GPU_CALL("draw.sprite", I(SURF_SCREEN), F(0), F(0), I(a), I(0));
        GPU_CALL("draw.sprite", I(SURF_SCREEN), F(4), F(4), I(b), I(0));
        GPU_CALL("draw.sprite", I(SURF_SCREEN), F(0), F(6), I(big), I(0));
        GPU_CALL("display.flip");

        if (!frame) {
            CHECK(ops(FIRST "TUPNCUULDUDTDSE"));
            CHECK(arg(QX_LIST_UPLOAD, 2, 1) == GPU_PAGE_FIRST && arg(QX_LIST_UPLOAD, 2, 2) == 1);
            CHECK(arg(QX_LIST_UPLOAD, 3, 2) == 3 && arg(QX_LIST_UPLOAD, 3, 3) == 0);
            CHECK(arg(QX_LIST_TEXTURE, 3, 1) == (uint32_t)big);
        }
    }

    CHECK(ops(HEAD "PNCDDSE") && arg(QX_LIST_DRAW, 1, 7) == (uint32_t)big);
    CHECK(arg(QX_LIST_DRAW, 0, 7) == GPU_PAGE_FIRST && arg(QX_LIST_DRAW, 0, 5) == 2 * 6);
    CHECK(vertices[0].rx == 1 && vertices[4].rx == 3 && vertices[4].ry == 0);

    screen(S, 8, 8, 8);
    const int32_t c = GPU_CALL("surface.new", I(2), I(2)).result.i;
    GPU_CALL("draw.sprite", I(SURF_SCREEN), F(0), F(0), I(c), I(0));
    GPU_CALL("surface.free", I(c));
    const int32_t d = GPU_CALL("surface.new", I(4), I(4)).result.i;
    GPU_CALL("draw.sprite", I(SURF_SCREEN), F(0), F(0), I(d), I(0));
    GPU_CALL("display.flip");
    CHECK(arg(QX_LIST_UPLOAD, 3, 1) == GPU_PAGE_FIRST + 1 && arg(QX_LIST_UPLOAD, 3, 2) == 0);
    CHECK(arg(QX_LIST_UPLOAD, 3, 4) == 4 && arg(QX_LIST_DRAW, 1, 7) == GPU_PAGE_FIRST + 1);
}

/* A texture written sends only the rows written since it was last sent, through a view as well;
   a new place sends it whole. */
TEST(gfx_gpu_sends_the_rows_written)
{
    screen(S, 8, 8, 8);
    const int32_t s = GPU_CALL("surface.new", I(4), I(6)).result.i;
    const int32_t v = GPU_CALL("surface.new_view", I(s), I(1), I(2), I(2), I(3)).result.i;
    uint8_t texels[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    QxArray a = gfx_array(texels, 8, 1);

    GPU_CALL("draw.sprite", I(SURF_SCREEN), F(0), F(0), I(s), I(0));
    GPU_CALL("display.flip");
    CHECK(arg(QX_LIST_UPLOAD, 2, 3) == 0 && arg(QX_LIST_UPLOAD, 2, 5) == 6);

    GPU_CALL("surface.write", I(s), I(0), I(4), I(4), I(1), ARRAY(&a));
    GPU_CALL("surface.write", I(v), I(0), I(0), I(2), I(1), ARRAY(&a));
    GPU_CALL("draw.sprite", I(SURF_SCREEN), F(0), F(0), I(s), I(0));
    GPU_CALL("display.flip");
    CHECK(ops(HEAD "PNCUDSE") && arg(QX_LIST_UPLOAD, 0, 1) == GPU_PAGE_FIRST + 1);
    CHECK(arg(QX_LIST_UPLOAD, 0, 3) == 2 && arg(QX_LIST_UPLOAD, 0, 4) == 4);
    CHECK(arg(QX_LIST_UPLOAD, 0, 5) == 3 && arg(QX_LIST_UPLOAD, 0, 6) == 4);

    GPU_CALL("draw.sprite", I(SURF_SCREEN), F(0), F(0), I(s), I(0));
    GPU_CALL("display.flip");
    CHECK(ops(HEAD "PNCDSE"));
}

/* SMOOTH asks the shader to mix a texture's texels. */
TEST(gfx_gpu_smooths_textures)
{
    screen(S, 8, 8, 8);
    const int32_t s = GPU_CALL("surface.new", I(2), I(2)).result.i;

    GPU_CALL("draw.sprite", I(SURF_SCREEN), F(0), F(0), I(s), I(GFX_DRAW_SMOOTH));
    GPU_CALL("draw.sprite", I(SURF_SCREEN), F(0), F(0), I(s), I(0));
    GPU_CALL("display.flip");
    CHECK(
        vertices[0].stages ==
        (GPU_STAGE_INDEXED | GPU_STAGE_KEYED | GPU_STAGE_SMOOTH | GPU_STAGE_EIGHT)
    );
    CHECK(vertices[4].stages == (GPU_STAGE_INDEXED | GPU_STAGE_KEYED | GPU_STAGE_EIGHT));
}

/* An 8-bit texture goes into an index page, sent once and again when written; its place freed
   with it. */
TEST(gfx_gpu_sends_a_texture_once)
{
    screen(S, 8, 8, 8);
    const int32_t s = GPU_CALL("surface.new", I(2), I(2)).result.i;
    uint8_t texels[] = {0, 5, 6, 7};

    QxArray a = gfx_array(texels, 4, 1);
    GPU_CALL("surface.write", I(s), I(0), I(0), I(2), I(2), ARRAY(&a));

    GPU_CALL("draw.sprite", I(SURF_SCREEN), F(1), F(1), I(s), I(GFX_DRAW_FLIP_X));
    GPU_CALL("display.flip");
    CHECK(ops(FIRST "TUPNCUTULDSE"));
    CHECK(
        arg(QX_LIST_TEXTURE, 3, 1) == GPU_PAGE_FIRST + 1 &&
        arg(QX_LIST_TEXTURE, 3, 4) == QX_TEXTURE_INDEX
    );
    CHECK(arg(QX_LIST_DRAW, 0, 7) == GPU_PAGE_FIRST + 1);
    CHECK(placed(&vertices[0], 0.5f, 0.5f) && vertices[0].u == 2 && vertices[1].u == 0);
    CHECK(vertices[0].stages == (GPU_STAGE_INDEXED | GPU_STAGE_KEYED | GPU_STAGE_EIGHT));
    CHECK(vertices[0].rw == 2);

    GPU_CALL("draw.sprite", I(SURF_SCREEN), F(1), F(1), I(s), I(0));
    GPU_CALL("display.flip");
    CHECK(ops(HEAD "PNCDSE"));

    GPU_CALL("surface.write", I(s), I(0), I(0), I(2), I(2), ARRAY(&a));
    GPU_CALL("draw.sprite", I(SURF_SCREEN), F(1), F(1), I(s), I(0));
    GPU_CALL("display.flip");
    CHECK(ops(HEAD "PNCUDSE"));

    GPU_CALL("surface.free", I(s));
    GPU_CALL("display.flip");
    CHECK(ops(HEAD "SE"));
}

/* A surface drawn into is an RGBA target of its own; an 8-bit one drawn from first leaves its
   page, and holds and is drawn from as indices, in alpha, keyed. */
TEST(gfx_gpu_draws_into_surfaces)
{
    screen(S, 8, 8, 8);
    const int32_t s = GPU_CALL("surface.new", I(2), I(2)).result.i;
    GPU_CALL("draw.sprite", I(SURF_SCREEN), F(0), F(0), I(s), I(0));
    GPU_CALL("draw.clear", I(s), I(4));
    GPU_CALL("draw.sprite", I(SURF_SCREEN), F(0), F(0), I(s), I(0));
    GPU_CALL("display.flip");
    CHECK(ops(FIRST "TUPNCUTULDTPNCDPNCDSE"));
    CHECK(
        arg(QX_LIST_TEXTURE, 4, 1) == (uint32_t)s && arg(QX_LIST_TEXTURE, 4, 4) == QX_TEXTURE_RGBA
    );
    CHECK(arg(QX_LIST_PASS, 1, 1) == (uint32_t)s && arg(QX_LIST_DRAW, 2, 7) == (uint32_t)s);
    CHECK(arg(QX_LIST_CLIP, 1, 3) == 2);
    CHECK(
        vertices[8].stages ==
        (GPU_STAGE_INDEXED | GPU_STAGE_ALPHA | GPU_STAGE_KEYED | GPU_STAGE_EIGHT)
    );
    CHECK(vertices[4].r == 4 && vertices[4].g == 4 && vertices[4].b == 4);
}

/* Plain draws take the simple shader: SOLID's, texels unlit into 32 bits, indices through row 0
   into 8; lit or tinted ones, indices into 32 bits, another row, blends and SMOOTH the whole. */
TEST(gfx_gpu_picks_the_simple_shader)
{
    const GpuVertex white = {
        .r = 255,
        .g = 255,
        .b = 255,
        .light = 63,
    };

    GpuVertex v = white;

    v.r = 7;
    v.stages = GPU_STAGE_SOLID | GPU_STAGE_KEYED | GPU_STAGE_EIGHT;
    CHECK(pipeline_kind(&v) == PIPELINE_SIMPLE);
    v.stages = GPU_STAGE_SOLID | GPU_STAGE_MIX;
    CHECK(pipeline_kind(&v) == PIPELINE_PLAIN);
    v.stages = GPU_STAGE_SOLID | GPU_STAGE_SMOOTH;
    CHECK(pipeline_kind(&v) == PIPELINE_PLAIN);

    v = white;
    v.stages = GPU_STAGE_KEYED | GPU_STAGE_WRAP;
    CHECK(pipeline_kind(&v) == PIPELINE_SIMPLE);
    v.light = 62;
    CHECK(pipeline_kind(&v) == PIPELINE_PLAIN);

    v = white;
    v.stages = GPU_STAGE_INDEXED;
    CHECK(pipeline_kind(&v) == PIPELINE_PLAIN);
    v.stages = GPU_STAGE_INDEXED | GPU_STAGE_EIGHT;
    CHECK(pipeline_kind(&v) == PIPELINE_SIMPLE);
    v.row = 1;
    CHECK(pipeline_kind(&v) == PIPELINE_PLAIN);
}

/* An 8-bit canvas holds each pixel's colour in row 0 and is shown as it is. After row 0 changed,
   the flip colours it again: its indices copied behind and drawn back through the row. Each flip
   drew it through row 0 again, a pass a frame for nothing. */
TEST(gfx_gpu_shows_8_bits_through_row_0)
{
    const uint16_t copy = GPU_STAGE_INDEXED | GPU_STAGE_ALPHA | GPU_STAGE_EIGHT;
    int32_t row[256] = {[1] = 0x404040};

    QxArray a = gfx_array(row, 256, 4);

    screen(S, 8, 8, 8);
    GPU_CALL("display.flip");
    CHECK(ops(FIRST "USE") && arg(QX_LIST_SHOW, 0, 1) == SURF_SCREEN);
    GPU_CALL("display.flip");
    CHECK(ops(HEAD "SE"));

    GPU_CALL("display.set_palette", I(0), ARRAY(&a));
    GPU_CALL("display.flip");
    CHECK(ops(HEAD "UTPNCLDPNCDSE") && arg(QX_LIST_PASS, 0, 1) == GPU_BEHIND);
    CHECK(arg(QX_LIST_DRAW, 0, 7) == SURF_SCREEN && arg(QX_LIST_PASS, 1, 1) == SURF_SCREEN);
    CHECK(arg(QX_LIST_DRAW, 1, 7) == GPU_BEHIND && arg(QX_LIST_SHOW, 0, 1) == SURF_SCREEN);
    CHECK(vertices[0].stages == copy && vertices[4].stages == copy && vertices[4].row == 0);
    GPU_CALL("display.flip");
    CHECK(ops(HEAD "SE"));
}

/* At 8 bits ADD, as BLEND, reads a copy of behind, and the palette's map, sent below its rows
   once: not again while row 0 stays, for a lit shape after. */
TEST(gfx_gpu_blends_8_bits_through_the_map)
{
    screen(S, 8, 8, 8);
    GPU_CALL("draw.rect", I(SURF_SCREEN), F(0), F(0), F(1), F(1), I(1), I(GFX_DRAW_FILL));
    GPU_CALL("display.flip");
    CHECK(ops(FIRST "TUPNCULDSE"));

    GPU_CALL("draw.rect", I(SURF_SCREEN), F(0), F(0), F(1), F(1), I(1), I(GFX_DRAW_ADD));
    GPU_CALL("display.flip");
    CHECK(ops(HEAD "PNCUTPNCDPNCLDSE") && arg(QX_LIST_UPLOAD, 0, 3) == PAL_ROWS);
    CHECK(arg(QX_LIST_UPLOAD, 0, 5) == GPU_MAP_ROWS && arg(QX_LIST_DRAW, 1, 9) == GPU_BEHIND);
    CHECK(vertices[4].stages == (GPU_STAGE_SOLID | GPU_STAGE_ADD | GPU_STAGE_EIGHT));

    GPU_CALL("draw.set_light", I(20));
    GPU_CALL("draw.rect", I(SURF_SCREEN), F(0), F(0), F(1), F(1), I(1), I(GFX_DRAW_FILL));
    GPU_CALL("display.flip");
    CHECK(ops(HEAD "PNCDSE"));
}

/* A read plays what was drawn and reads it back: 32 bits as they are; 8 bits the index in alpha,
   whatever its colour. Read from red, an 8-bit pixel read back as its colour's red. */
TEST(gfx_gpu_reads_back)
{
    screen(S, 4, 1, 32);
    GPU_CALL("draw.pset", I(SURF_SCREEN), F(0), F(0), I(0x123456));
    memcpy(pixels, (const uint8_t[]){0x12, 0x34, 0x56, 0xFF}, 4);

    CHECK(GPU_CALL("surface.point", I(SURF_SCREEN), I(0), I(0)).result.i == 0x123456);
    CHECK(plays == 1 && ops(FIRST "TUPNCULDE"));
    CHECK(GPU_CALL("surface.point", I(SURF_SCREEN), I(1), I(0)).result.i == 0);
    CHECK(plays == 1);

    screen(S, 4, 1, 8);
    GPU_CALL("draw.pset", I(SURF_SCREEN), F(0), F(0), I(4));
    memcpy(pixels, (const uint8_t[]){9, 9, 9, 4, 7, 7, 7, 201}, 8);

    CHECK(GPU_CALL("surface.point", I(SURF_SCREEN), I(0), I(0)).result.i == 4);
    CHECK(GPU_CALL("surface.point", I(SURF_SCREEN), I(1), I(0)).result.i == 201);
}

/* Root h's depth texture's id. */
static uint32_t depth_of(const uint32_t h) { return h | (uint32_t)GPU_DEPTH << HDL_KIND_SHIFT; }

/* A quad 1 near at its top, 4 at its foot, drawn from a 2 by 2 texture with flags. */
static void deep_quad(Session *const S, const int32_t flags)
{
    const int32_t s = GPU_CALL("surface.new", I(2), I(2)).result.i;
    gfx_draw_vertex p[4] = {{0, 0, 1, 0, 0}, {7, 0, 1, 2, 0}, {7, 7, 4, 2, 2}, {0, 7, 4, 0, 2}};

    QxArray a = gfx_array(p, 4, sizeof *p);
    GPU_CALL("draw.poly", I(SURF_SCREEN), ARRAY(&a), I(0), I(4), I(s), I(0), I(flags));
    GPU_CALL("display.flip");
}

/* With depth, the target's depth texture is made and its pass draws into it, through a pipeline
   testing and setting depth; a corner's depth is 1/z, and in perspective its w is z, so the GPU
   steps texels and depth as software's scanner. The GPU drew polys affine and without depth: a
   cube's far faces over its near ones, its textures bent along each triangle's edge. */
TEST(gfx_gpu_draws_polys_in_perspective_by_depth)
{
    const uint32_t deep = depth_of(SURF_SCREEN);

    screen(S, 8, 8, 32);
    deep_quad(S, GFX_DRAW_PERSPECTIVE | GFX_DRAW_DEPTH_TEST | GFX_DRAW_DEPTH_WRITE);
    CHECK(ops(FIRST "TTUPNCUULDSE"));
    CHECK(arg(QX_LIST_TEXTURE, 2, 1) == deep && arg(QX_LIST_TEXTURE, 2, 4) == QX_TEXTURE_DEPTH);
    CHECK(arg(QX_LIST_TEXTURE, 2, 2) == 8 && arg(QX_LIST_TEXTURE, 2, 3) == 8);
    CHECK(arg(QX_LIST_PASS, 0, 1) == SURF_SCREEN && arg(QX_LIST_PASS, 0, 2) == deep);
    CHECK(arg(QX_LIST_PIPELINE, 0, 2) == (QX_DEPTH_TEST | QX_DEPTH_WRITE) << 8);
    CHECK(vertices[0].z == 1 && vertices[0].w == 1);
    CHECK(vertices[2].z == 0.25f && vertices[2].w == 4);

    screen(S, 8, 8, 32);
    deep_quad(S, GFX_DRAW_DEPTH_TEST);
    CHECK(arg(QX_LIST_PIPELINE, 0, 2) == QX_DEPTH_TEST << 8);
    CHECK(vertices[2].z == 0.25f && vertices[2].w == 1); /* affine */
}

/* clear_depth on a depth texture just made draws nothing, it being all 0; again, a box over the
   surface writing depth 0 and no colour. */
TEST(gfx_gpu_clears_depth)
{
    screen(S, 8, 8, 32);
    GPU_CALL("surface.clear_depth", I(SURF_SCREEN));
    GPU_CALL("surface.clear_depth", I(SURF_SCREEN));
    GPU_CALL("display.flip");
    CHECK(ops(FIRST "TTUPNCULDSE") && arg(QX_LIST_TEXTURE, 2, 4) == QX_TEXTURE_DEPTH);
    CHECK(arg(QX_LIST_PIPELINE, 0, 2) == (QX_BLEND_KEEP | QX_DEPTH_WRITE << 8));
    CHECK(arg(QX_LIST_DRAW, 0, 5) == 6 && vertices[0].z == 0);
    CHECK(placed(&vertices[0], -0.5f, -0.5f) && placed(&vertices[2], 7.5f, 7.5f));
}
