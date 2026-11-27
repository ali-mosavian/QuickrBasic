/* plat/dos32/accel/file.c: a driver file's IDs read, as the loader reads one for each card. */
#include "plat/dos32/accel/accel.h"
#include "plat/dos32/accel/cut.h"
#include "qx/qxlist.h"
#include <stddef.h>
#include "dll.h"
#include "unit.h"
#include <string.h>
#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

static const char *const DRIVER[] = {"accel_init", "accel_ids"};
static const uint32_t DRIVER_AT[] = {0x1004, DLL_DATA};

/* A driver file of Voodoo 1 and 2's IDs in file: its bytes. */
static uint32_t voodoo(uint8_t *const file)
{
    const uint32_t n = dll_make(file, DRIVER, DRIVER_AT, 2);
    store32(dll_at(file, DLL_DATA), ACCEL_ID(0x121A, 0x0001));
    store32(dll_at(file, DLL_DATA + 4), ACCEL_ID(0x121A, 0x0002));
    return n;
}

/* A driver names the cards it drives by vendor and device: a Banshee's is not among them. */
TEST(accel_driver_files_name_their_cards)
{
    const PciFunction one = {.vendor_id = 0x121A, .device_id = 0x0001};

    const PciFunction two = {.vendor_id = 0x121A, .device_id = 0x0002};

    const PciFunction banshee = {.vendor_id = 0x121A, .device_id = 0x0003};

    uint8_t file[DLL_BYTES];
    const uint32_t n = voodoo(file);

    CHECK(accel_drives(file, n, &one) && accel_drives(file, n, &two));
    CHECK(!accel_drives(file, n, &banshee));
}

/* A file read short, or a DLL without accel_ids, names none; the IDs end at the first 0. */
TEST(accel_driver_files_are_read_whole_or_not_at_all)
{
    const PciFunction two = {.vendor_id = 0x121A, .device_id = 0x0002};

    const PciFunction banshee = {.vendor_id = 0x121A, .device_id = 0x0003};

    uint8_t file[DLL_BYTES];
    const uint32_t n = voodoo(file);

    CHECK(!accel_drives(file, n - 1, &two));
    store32(dll_at(file, DLL_DATA + 12), ACCEL_ID(0x121A, 0x0003));
    CHECK(!accel_drives(file, n, &banshee));
    dll_make(file, DRIVER, DRIVER_AT, 1);
    CHECK(!accel_drives(file, n, &two));
}

/* ---- the triangle cut at tiles and wraps (cut.c) ---- */

enum { MOST_PIECES = 16 };

static struct {
    uint32_t n;
    uint32_t column[MOST_PIECES], corners[MOST_PIECES];
    AccelVertex v[MOST_PIECES][CUT_CORNERS];
} cut;

static void collected(void *ctx, uint32_t column, uint32_t row, const AccelVertex *v, uint32_t n)
{
    (void)ctx, (void)row;

    if (cut.n == MOST_PIECES)
        return;

    cut.column[cut.n] = column;
    cut.corners[cut.n] = n;
    memcpy(cut.v[cut.n++], v, n * sizeof *v);
}

static bool near(const float a, const float b) { return a - b < 1e-3f && b - a < 1e-3f; }

/* A triangle in perspective across a seam at s 256: cut where its texels reach 256, not where its
   screen's half way would, each piece's texels the tile's own. */
TEST(accel_cut_meets_a_seam_where_the_texels_do)
{
    const CutGrid g = {
        .tile_w = 256,
        .tile_h = 32,
        .across = 2,
        .down = 1,
    };

    const AccelVertex v[3] = {
        {0, 0, 0, 1, 0, 0, 0xFFFFFFFF},
        {100, 0, 0, 0.5f, 300, 0, 0xFFFFFFFF},
        {0, 10, 0, 1, 0, 0, 0xFFFFFFFF},
    };

    bool met = false;

    cut = (typeof(cut)){};

    cut_triangle(&g, v, collected, nullptr);
    CHECK(cut.n == 2 && cut.column[0] == 0 && cut.column[1] == 1);

    for (uint32_t i = 0; i < cut.corners[1]; i++) {
        const AccelVertex *const c = &cut.v[1][i];
        CHECK(c->s > -1e-3f && c->s < 44 + 1e-3f);
        met = met || (near(c->s, 0) && near(c->x, 100.0f * 256 / 278));
    }

    CHECK(met);
}

/* A wrap of texels 4 to 12 across s 4 to 28: three periods, each moved into the first. */
TEST(accel_cut_moves_each_period_of_a_wrap_into_the_first)
{
    const CutGrid g = {
        .tile_w = 16,
        .tile_h = 16,
        .across = 1,
        .down = 1,
        .wrap_s = true,
        .wrap = {4, 0, 8, 16},
    };

    const AccelVertex v[3] = {
        {0, 0, 0, 1, 4, 0, 0xFFFFFFFF},
        {24, 0, 0, 1, 28, 0, 0xFFFFFFFF},
        {0, 8, 0, 1, 4, 8, 0xFFFFFFFF},
    };

    cut = (typeof(cut)){};

    cut_triangle(&g, v, collected, nullptr);
    CHECK(cut.n == 3);

    for (uint32_t k = 0; k < cut.n; k++)
        for (uint32_t i = 0; i < cut.corners[k]; i++)
            CHECK(cut.v[k][i].s > 4 - 1e-3f && cut.v[k][i].s < 12 + 1e-3f);
}

/* MEADOW's sky: a texture 4 wide, a tile of 8 for the card's aspect, its wrap the player's. A
   triangle whose texels stay in one period is drawn as it is, its texels exact: cut, each corner's
   went through s q / q, 1.7 coming back 1.7000002, and the sky cost 35k cycles of a 486 frame. */
TEST(accel_cut_leaves_a_wrap_in_one_period_whole)
{
    const CutGrid g = {
        .tile_w = 8,
        .tile_h = 64,
        .across = 1,
        .down = 1,
        .wrap_s = true,
        .wrap_t = true,
        .wrap = {0, 0, 4, 64},
    };

    const AccelVertex v[3] = {
        {0, 0, 0, 0.7f, 1.7f, 0, 0xFFFFFFFF},
        {24, 0, 0, 0.7f, 4, 0, 0xFFFFFFFF},
        {0, 8, 0, 0.7f, 1.7f, 64, 0xFFFFFFFF},
    };

    cut = (typeof(cut)){};

    cut_triangle(&g, v, collected, nullptr);
    CHECK(cut.n == 1 && cut.corners[0] == 3 && !memcmp(cut.v[0], v, sizeof v));
}

/* ---- the player against a driver that records its calls (player.c) ---- */

enum { SCREEN_W = 320, SCREEN_H = 200, SLOTS = 8, TILE = 256 * 32, CALLS = 256 };

/* A call a letter: Texture, Upload, Forget, tarGet, Drop, Aim, State, triangles (V), shoW,
   Carry, Read, cLear, Indexed, Palette. */
static struct {
    char calls[CALLS];
    uint32_t ncalls;
    uint16_t texture_w[SLOTS], texture_h[SLOTS];
    bool indexed[SLOTS];
    uint32_t textures;
    AccelState state;      /* the last sent */
    uint32_t palette[256]; /* the last sent */
    uint16_t up[SLOTS][TILE];
    AccelVertex last[3]; /* the last polygon's last triangle */
    uint8_t sides;       /* its corners */
    uint32_t room, held; /* the targets the card has room for, and holds */
    uint32_t sent;       /* texels uploaded */
    uint32_t filled;     /* the last clear's colour */
    uint8_t what;        /* and what it cleared */
} fake;

static void called(const char c)
{
    if (fake.ncalls + 1 < CALLS)
        fake.calls[fake.ncalls++] = c;
}

static uint32_t calls_of(const char c)
{
    uint32_t n = 0;

    for (uint32_t i = 0; i < fake.ncalls; i++)
        n += fake.calls[i] == c;

    return n;
}

static bool ACCEL_CALL fake_open(const PciFunction *card, uint16_t w, uint16_t h, AccelCard *out)
{
    (void)card, (void)w, (void)h;
    *out = (AccelCard){
        .side = 256,
        .aspect = 3,
        .memory = 2 << 20,
        .millihz = 60000,
    };

    return true;
}

static void ACCEL_CALL fake_close(void) {}

static bool ACCEL_CALL fake_texture(uint32_t slot, uint16_t w, uint16_t h)
{
    called('T');

    if (slot >= SLOTS)
        return false;

    fake.texture_w[slot] = w;
    fake.texture_h[slot] = h;
    fake.textures++;
    return true;
}

static void ACCEL_CALL
fake_upload(uint32_t slot, AccelRect r, const uint16_t *texels, uint32_t pitch)
{
    called('U');
    fake.sent += (uint32_t)(r.w * r.h);

    for (int32_t y = 0; y < r.h; y++)
        memcpy(
            fake.up[slot] + (r.y + y) * fake.texture_w[slot] + r.x,
            texels + y * pitch,
            (size_t)r.w * 2
        );
}

static void ACCEL_CALL fake_forget(uint32_t slot) { (void)slot, called('F'); }

static bool ACCEL_CALL fake_target(uint32_t slot, uint16_t w, uint16_t h, bool depth)
{
    (void)slot, (void)w, (void)h, (void)depth;
    called('G');

    if (fake.held == fake.room)
        return false;

    fake.held++;
    return true;
}

static void ACCEL_CALL fake_drop(uint32_t slot)
{
    (void)slot, called('D');
    fake.held--;
}

static void ACCEL_CALL fake_aim(uint32_t slot, AccelRect clip)
{
    (void)slot, (void)clip, called('A');
}

static void ACCEL_CALL fake_state(const AccelState *s)
{
    called('S');
    fake.state = *s;
}

static bool ACCEL_CALL fake_indexed(uint32_t slot, uint16_t w, uint16_t h)
{
    const bool made = fake_texture(slot, w, h);

    fake.calls[fake.ncalls - 1] = 'I';

    if (made)
        fake.indexed[slot] = true;

    return made;
}

static void ACCEL_CALL fake_palette(const uint32_t *colours)
{
    called('P');
    memcpy(fake.palette, colours, sizeof fake.palette);
}

static void ACCEL_CALL fake_fans(const AccelVertex *v, const uint8_t *sides, uint32_t n)
{
    called('V');

    for (uint32_t i = 0; i + 1 < n; i++)
        v += sides[i];

    const uint8_t k = sides[n - 1];

    fake.last[0] = v[0];
    fake.last[1] = v[k - 2];
    fake.last[2] = v[k - 1];
    fake.sides = k;
}

static void ACCEL_CALL fake_show(bool vsync) { (void)vsync, called('W'); }

static void ACCEL_CALL fake_carry(void) { called('C'); }

static void ACCEL_CALL fake_read(uint32_t slot, AccelRect r, uint8_t *out)
{
    (void)slot;
    called('R');
    memset(out, 0x42, (size_t)r.w * r.h * 4);
}

static void ACCEL_CALL fake_clear(uint32_t argb, uint8_t what)
{
    called('L');
    fake.filled = argb;
    fake.what = what;
}

static const AccelDriver FAKE = {
    .size = sizeof(AccelDriver),
    .abi = ACCEL_ABI,
    .name = "fake",
    .open = fake_open,
    .close = fake_close,
    .texture = fake_texture,
    .upload = fake_upload,
    .forget = fake_forget,
    .target = fake_target,
    .drop = fake_drop,
    .aim = fake_aim,
    .state = fake_state,
    .fans = fake_fans,
    .show = fake_show,
    .carry = fake_carry,
    .read = fake_read,
    .clear = fake_clear,
    .indexed = fake_indexed,
    .palette = fake_palette,
};

/* A vertex as the FIXED pipeline's attributes find it. */
typedef struct {
    float x, y, z, w, u, v;
    uint16_t rx, ry, rw, rh;
    uint8_t r, g, b, a;
} Vertex;

enum {
    SHOWN = 100,
    ART = 7,
    SOLID = 1,
    TEXTURED = 2,
    MASKED = 3,
    VERTICES = 1,
    INDICES = 2,
    LIST = 1024
};

static struct {
    uint32_t words[LIST], n;
    Vertex v[64];
    uint32_t nv, index[96], ni;
    uint32_t depth; /* the pass's depth texture: 0, none */
} list;

static void put(const QxListOp op, const uint32_t *const a, const uint32_t n)
{
    list.words[list.n++] = op | (n + 1) << QX_LIST_SIZE;
    memcpy(list.words + list.n, a, n * sizeof *a);
    list.n += n;
}

#define PUT(op, ...)                                                                               \
    put(op, (const uint32_t[]){__VA_ARGS__}, sizeof((const uint32_t[]){__VA_ARGS__}) / 4)
#define POINTER(p) (uint32_t)(uintptr_t)(p), (uint32_t)((uint64_t)(uintptr_t)(p) >> 32)

static void blended(
    const uint32_t id,
    const QxFixedSource source,
    const uint32_t flags,
    const QxBlend blend
)
{
    PUT(QX_LIST_PIPELINE,
        id,
        blend,
        sizeof(Vertex),
        1,
        source,
        flags,
        0,
        0,
        0,
        0,
        QX_FIXED_AT_POSITION,
        QX_ATTRIBUTE_FLOAT4,
        offsetof(Vertex, x),
        QX_FIXED_AT_TEXEL,
        QX_ATTRIBUTE_FLOAT2,
        offsetof(Vertex, u),
        QX_FIXED_AT_COLOUR,
        QX_ATTRIBUTE_UBYTE4,
        offsetof(Vertex, r),
        QX_FIXED_AT_RECT,
        QX_ATTRIBUTE_USHORT4,
        offsetof(Vertex, rx));
}

static void pipeline(const uint32_t id, const QxFixedSource source, const uint32_t flags)
{
    blended(id, source, flags, QX_BLEND_NONE);
}

/* A new list: the screen's texture, two pipelines, the art's texture w by h. */
static void begun(const uint32_t w, const uint32_t h)
{
    list.n = list.nv = list.ni = 0;
    PUT(QX_LIST_TEXTURE, SHOWN, SCREEN_W, SCREEN_H, QX_TEXTURE_RGBA);
    PUT(QX_LIST_TEXTURE, ART, w, h, QX_TEXTURE_RGBA);
    pipeline(SOLID, QX_FIXED_COLOUR, 0);
    pipeline(TEXTURED, QX_FIXED_TEXEL, 0);
    pipeline(MASKED, QX_FIXED_MASK, QX_FIXED_KEY);
}

/* A quad over pixels x0 to x1, y0 to y1, texels u0 to u1 across, drawn with pipeline from
   texture at alpha, its colour red. */
static void box_at(
    const uint32_t pipe,
    const uint32_t texture,
    const float x0,
    const float y0,
    const float x1,
    const float y1,
    const float u0,
    const float u1,
    const uint8_t red,
    const uint32_t alpha
)
{
    const uint32_t at = list.nv, first = list.ni;
    const float xs[] = {x0, x1, x1, x0}, ys[] = {y0, y0, y1, y1}, us[] = {u0, u1, u1, u0};

    const uint32_t corners[] = {0, 1, 2, 0, 2, 3};

    for (uint32_t i = 0; i < 4; i++)
        list.v[list.nv++] =
            (Vertex){xs[i] - 0.5f, ys[i] - 0.5f, 0, 1, us[i], 0, .r = red, .a = 255};

    for (uint32_t i = 0; i < 6; i++)
        list.index[list.ni++] = at + corners[i];

    PUT(QX_LIST_DRAW, pipe, VERTICES, INDICES, first, 6, alpha, texture);
}

/* A quad as box_at's, red and opaque. */
static void box(
    const uint32_t pipe,
    const uint32_t texture,
    const float x0,
    const float y0,
    const float x1,
    const float y1,
    const float u0,
    const float u1
)
{
    box_at(pipe, texture, x0, y0, x1, y1, u0, u1, 255, 255);
}

/* The list ended: its buffers, a pass into into before its draws, shown when show. */
static bool played(const uint32_t into, const uint32_t draws_from, const bool show)
{
    uint32_t words[LIST], n = 0;

    const uint32_t head[] = {
        QX_LIST_BUFFER | 4 << QX_LIST_SIZE,
        VERTICES,
        (uint32_t)sizeof list.v,
        QX_BUFFER_VERTEX,
        QX_LIST_WRITE | 5 << QX_LIST_SIZE,
        VERTICES,
        list.nv * (uint32_t)sizeof *list.v,
        POINTER(list.v),
        QX_LIST_BUFFER | 4 << QX_LIST_SIZE,
        INDICES,
        (uint32_t)sizeof list.index,
        QX_BUFFER_INDEX,
        QX_LIST_WRITE | 5 << QX_LIST_SIZE,
        INDICES,
        list.ni * 4,
        POINTER(list.index),
        QX_LIST_PASS | (list.depth ? 3 : 2) << QX_LIST_SIZE,
        into,
    };

    memcpy(words, list.words, draws_from * 4);
    n = draws_from;
    memcpy(words + n, head, sizeof head);
    n += sizeof head / 4;

    if (list.depth)
        words[n++] = list.depth;

    memcpy(words + n, list.words + draws_from, (list.n - draws_from) * 4);
    n += list.n - draws_from;

    if (show) {
        words[n++] = QX_LIST_SHOW | 2 << QX_LIST_SIZE;
        words[n++] = SHOWN;
    }

    words[n++] = QX_LIST_END | 1 << QX_LIST_SIZE;
    return accel_play(words, n);
}

/* The fake card opened, its screen's flags QxScreen's. */
static void opened_as(const uint8_t flags)
{
    fake = (typeof(fake)){.room = ACCEL_TARGETS};

    CHECK(accel_open(&FAKE, &(PciFunction){}, SCREEN_W, SCREEN_H, flags));
}

static void opened(void) { opened_as(QX_SCREEN_VSYNC); }

/* Two draws alike: the state sent once, before the first's triangles. */
TEST(accel_player_sends_state_only_as_it_changes)
{
    opened();
    begun(16, 16);

    const uint32_t from = list.n;

    box(TEXTURED, ART, 0, 0, 16, 16, 0, 16);
    box(TEXTURED, ART, 20, 0, 36, 16, 0, 16);
    CHECK(played(SHOWN, from, true));
    CHECK(calls_of('S') == 1 && calls_of('T') == 1 && calls_of('W') == 1);
    accel_close();
}

/* A quad's two triangles, a, b, c and a, c, d by their indices, go to the driver as one polygon:
   sent as two, each corner was copied twice more and the driver found the quad again by comparing
   corners word by word, 50k cycles of MEADOW's 486 frame. */
TEST(accel_player_sends_a_quads_two_triangles_as_one_polygon)
{
    opened();
    begun(16, 16);

    const uint32_t from = list.n;

    box_at(SOLID, 0, 2, 4, 10, 12, 0, 0, 5, 255);
    CHECK(played(SHOWN, from, true) && calls_of('V') == 1 && fake.sides == 4);
    CHECK(fake.last[1].x == 10 - 0.5f && fake.last[1].y == 12 - 0.5f);
    CHECK(fake.last[2].x == 2 - 0.5f && fake.last[2].y == 12 - 0.5f);
    accel_close();
}

/* An RGBA texel keyed at a half alpha and up; each channel's top five bits kept. */
TEST(accel_player_keys_texels_from_half_alpha)
{
    const uint8_t texels[] = {255, 255, 255, 127, 8, 16, 248, 128};

    opened();
    begun(2, 1);
    PUT(QX_LIST_UPLOAD, ART, 0, 0, 2, 1, 8, POINTER(texels));

    const uint32_t from = list.n;

    box(TEXTURED, ART, 0, 0, 2, 1, 0, 2);
    CHECK(played(SHOWN, from, true));
    CHECK(fake.up[1][0] == 0x7FFF && fake.up[1][1] == (0x8000 | 1 << 10 | 2 << 5 | 31));
    accel_close();
}

/* A texture wider than the card's side: two tiles of 256 by 32 (its aspect at most 8), the
   second repeating the texture's last column and row once past them. */
TEST(accel_player_tiles_a_texture_wider_than_the_card)
{
    static uint8_t texels[300 * 10 * 4];

    for (uint32_t i = 0; i < 300 * 10; i++)
        texels[4 * i + 2] = i % 300 == 299 ? 248 : 0; /* the last column blue */

    texels[4 * (9 * 300 + 299) + 1] = 248; /* its last texel cyan */

    opened();
    begun(300, 10);
    PUT(QX_LIST_UPLOAD, ART, 0, 0, 300, 10, 1200, POINTER(texels));

    const uint32_t from = list.n;

    box(TEXTURED, ART, 0, 0, 300, 10, 0, 300);
    CHECK(played(SHOWN, from, true));
    CHECK(fake.textures == 2 && fake.texture_w[1] == 256 && fake.texture_h[1] == 32);
    CHECK(fake.up[2][43] == 31 && fake.up[2][44] == 31 && fake.up[2][42] == 0);
    CHECK(fake.up[2][10 * 256 + 44] == (31 << 5 | 31));
    accel_close();
}

/* A texture's pixels read from its texels until drawn into, then from the card's target. */
TEST(accel_player_reads_back_what_the_card_drew)
{
    const uint8_t texel[] = {0, 0, 255, 255};

    uint8_t out[4];

    opened();
    begun(1, 1);
    PUT(QX_LIST_UPLOAD, ART, 0, 0, 1, 1, 4, POINTER(texel));
    played(SHOWN, list.n, false);
    accel_read(ART, 0, 0, 1, 1, out);
    CHECK(calls_of('R') == 0 && out[0] == 0 && out[2] == 255 && out[3] == 255);

    list.n = list.nv = list.ni = 0;
    box(SOLID, 0, 0, 0, 1, 1, 0, 0);
    CHECK(!played(ART, 0, false));
    accel_read(ART, 0, 0, 1, 1, out);
    CHECK(calls_of('R') == 1 && out[0] == 0x42);
    accel_close();
}

/* A frame shown leaves the next undefined: carried when first drawn in, unless a draw covers all of
   it opaque. */
TEST(accel_player_carries_a_frame_only_when_not_drawn_over)
{
    opened();
    begun(1, 1);

    const uint32_t from = list.n;

    box(SOLID, 0, 0, 0, 8, 8, 0, 0);
    CHECK(played(SHOWN, from, true) && calls_of('C') == 0 && calls_of('V') == 1);

    list.n = list.nv = list.ni = 0;
    box(SOLID, 0, 0, 0, 8, 8, 0, 0);
    CHECK(played(SHOWN, 0, true) && calls_of('C') == 1);

    list.n = list.nv = list.ni = 0;
    box(SOLID, 0, 0, 0, SCREEN_W, SCREEN_H, 0, 0);
    CHECK(played(SHOWN, 0, true) && calls_of('C') == 1);
    accel_close();
}

/* A screen opened with DISCARD keeps nothing past a SHOW: its next frame is never carried. MEADOW
   drew all of its screen each frame and spent two thirds of a Voodoo 1's frame carrying the last. */
TEST(accel_player_carries_nothing_on_a_discarded_screen)
{
    opened_as(QX_SCREEN_VSYNC | QX_SCREEN_DISCARD);
    begun(1, 1);

    const uint32_t from = list.n;

    box(SOLID, 0, 0, 0, 8, 8, 0, 0);
    CHECK(played(SHOWN, from, true));

    list.n = list.nv = list.ni = 0;
    box(SOLID, 0, 0, 0, 8, 8, 0, 0);
    CHECK(played(SHOWN, 0, true) && calls_of('C') == 0);
    accel_close();
}

/* Bytes the C heap holds. */
static size_t heap_used(void)
{
#ifdef __APPLE__
    malloc_statistics_t s;
    malloc_zone_statistics(nullptr, &s);
    return s.size_in_use;
#else
    return mallinfo2().uordblks;
#endif
}

/* The screen is drawn by the card and never drawn from, so it holds no texels here: held, a 640 by
   480 screen's 600K did not fit a 16 MB machine's heap, and nothing was shown. */
TEST(accel_player_holds_no_texels_for_the_screen)
{
    const size_t before = heap_used();

    opened();
    begun(1, 1);

    const uint32_t from = list.n;

    box(SOLID, 0, 0, 0, 8, 8, 0, 0);
    CHECK(played(SHOWN, from, true));
    CHECK(heap_used() - before < SCREEN_W * SCREEN_H * sizeof(uint16_t));
    accel_close();
}

/* A list may draw into the screen before its first SHOW, as gfx's does when it reads back: the
   draws go to the card's screen. Found only by that SHOW, the screen was a target first, which a
   card's spare rows did not hold, and read back once found. */
TEST(accel_player_draws_into_the_screen_before_its_first_show)
{
    opened();
    begun(1, 1);

    const uint32_t from = list.n;

    box(SOLID, 0, 0, 0, 8, 8, 0, 0);
    CHECK(!played(SHOWN, from, false));
    list.n = list.nv = list.ni = 0;
    CHECK(played(SHOWN, 0, true));
    CHECK(calls_of('G') == 0 && calls_of('R') == 0 && calls_of('V') == 1 && calls_of('W') == 1);
    accel_close();
}

/* A MASK draw is drawn from its texture, as TEXEL's: the card keys the colour by its texels. */
TEST(accel_player_draws_a_mask_from_its_texture)
{
    const uint8_t texel[] = {0, 0, 0, 255};

    opened();
    begun(1, 1);
    PUT(QX_LIST_UPLOAD, ART, 0, 0, 1, 1, 4, POINTER(texel));

    const uint32_t from = list.n;

    box(MASKED, ART, 0, 0, 8, 8, 0, 1);
    CHECK(played(SHOWN, from, true));
    CHECK(calls_of('T') == 1 && calls_of('U') == 1 && calls_of('V') == 1);
    accel_close();
}

/* With room for one target, a second is made by pulling the first's pixels into its texels and
   giving its room back. Not, the second's draws were lost: CUBE showed one face of six. */
TEST(accel_player_makes_room_for_a_target_from_the_oldest)
{
    enum { OTHER = 8 };

    opened();
    fake.room = 1;
    begun(1, 1);
    PUT(QX_LIST_TEXTURE, OTHER, 1, 1, QX_TEXTURE_RGBA);

    const uint32_t from = list.n;

    box(SOLID, 0, 0, 0, 1, 1, 0, 0);
    CHECK(!played(ART, from, false) && calls_of('L') == 1);

    list.n = list.nv = list.ni = 0;
    box(SOLID, 0, 0, 0, 1, 1, 0, 0);
    CHECK(!played(OTHER, 0, false) && calls_of('L') == 2 && calls_of('R') == 1);

    list.n = list.nv = list.ni = 0;
    box(TEXTURED, ART, 0, 0, 8, 8, 0, 1);
    CHECK(played(SHOWN, 0, true) && calls_of('V') == 1);
    CHECK(fake.up[1][0] == (8 << 10 | 8 << 5 | 8)); /* the 0x42s read back, as 1555 */
    accel_close();
}

/* A 300 by 10 texture in two 256 by 32 tiles sends the card its texels and the row and column a
   filter reads past them, rounded to even: 256 + 46 columns by 11 rows. The tiles whole were 6x
   that, read back and sent again for each text a frame: CUBE's text took 78% of a P100's frame. */
TEST(accel_player_sends_only_the_texels_a_draw_reaches)
{
    static uint8_t texels[300 * 10 * 4];

    opened();
    begun(300, 10);
    PUT(QX_LIST_UPLOAD, ART, 0, 0, 300, 10, 1200, POINTER(texels));

    const uint32_t from = list.n;

    box(TEXTURED, ART, 0, 0, 300, 10, 0, 300);
    CHECK(played(SHOWN, from, true));
    CHECK(fake.sent == (256 + 46) * 11);
    accel_close();
}

/* A solid rectangle over the whole target is the card's fill, not two triangles: CUBE's clear,
   a full-screen pass a frame, is 300K pixels at a Voodoo 1's pixel a clock. */
TEST(accel_player_clears_by_the_cards_fill)
{
    opened();
    begun(1, 1);

    const uint32_t from = list.n;

    box(SOLID, 0, 0, 0, SCREEN_W, SCREEN_H, 0, 0);
    CHECK(played(SHOWN, from, true));
    CHECK(calls_of('L') == 1 && calls_of('V') == 0 && calls_of('C') == 0);
    CHECK(fake.what == ACCEL_CLEAR_COLOUR && fake.filled == 0xFFFF0000);
    accel_close();
}

/* A pass's new depth buffer is cleared by the card's fill, not a full-screen quad. */
TEST(accel_player_clears_depth_by_the_cards_fill)
{
    enum { DEPTH = 9 };

    opened();
    begun(1, 1);
    PUT(QX_LIST_TEXTURE, DEPTH, SCREEN_W, SCREEN_H, QX_TEXTURE_DEPTH);
    list.depth = DEPTH;
    CHECK(played(SHOWN, list.n, true));
    list.depth = 0;
    CHECK(calls_of('L') == 1 && calls_of('V') == 0 && fake.what == ACCEL_CLEAR_DEPTH);
    accel_close();
}

/* ---- INDEX textures (indices.h) ---- */

enum { INDEXED = 11, OTHER = 12, MIXED = 4 };

/* A new list: the screen INDEX, an INDEX art w by h holding 0 up, OTHER an INDEX target 2 by 2,
   COLOURS index i i times &H010101 but 3 &H424242, LOOKUP index i i + 1. */
static void indexed_begun(const uint32_t w, const uint32_t h)
{
    static uint8_t art[64];
    static uint32_t colours[256], lookup[256];

    for (uint32_t i = 0; i < 256; i++) {
        colours[i] = i * 0x010101;
        lookup[i] = (i + 1) & 0xFF;
    }

    for (uint32_t i = 0; i < sizeof art; i++)
        art[i] = (uint8_t)i;

    colours[3] = 0x424242;
    list.n = list.nv = list.ni = 0;
    PUT(QX_LIST_TEXTURE, SHOWN, SCREEN_W, SCREEN_H, QX_TEXTURE_INDEX);
    PUT(QX_LIST_TEXTURE, INDEXED, w, h, QX_TEXTURE_INDEX);
    PUT(QX_LIST_TEXTURE, OTHER, 2, 2, QX_TEXTURE_INDEX);
    PUT(QX_LIST_UPLOAD, INDEXED, 0, 0, w, h, w, POINTER(art));
    PUT(QX_LIST_COLOURS, POINTER(colours));
    PUT(QX_LIST_LOOKUP, POINTER(lookup));
    pipeline(SOLID, QX_FIXED_COLOUR, 0);
    pipeline(TEXTURED, QX_FIXED_TEXEL, QX_FIXED_KEY | QX_FIXED_LINEAR);
    blended(MIXED, QX_FIXED_COLOUR, 0, QX_BLEND_MIX);
}

/* An INDEX texture's tile is the driver's indexed one, at least four texels across, its texels
   its indices. Into the screen it draws through COLOURS of LOOKUP's index, its texel alone, keyed
   but not filtered across the key; the palette sent once while the same. */
TEST(accel_player_draws_an_index_texture_through_the_shown_colours)
{
    opened();
    indexed_begun(3, 1);

    const uint32_t from = list.n;

    box(TEXTURED, INDEXED, 0, 0, 3, 1, 0, 3);
    box(TEXTURED, INDEXED, 4, 0, 7, 1, 0, 3);
    CHECK(played(SHOWN, from, true));
    CHECK(calls_of('I') == 1 && fake.indexed[1] && fake.texture_w[1] == 4);
    CHECK(fake.up[1][1] == 1 && fake.up[1][2] == 2 && fake.up[1][3] == 2);
    CHECK(calls_of('P') == 1 && fake.palette[2] == 0x424242 && fake.palette[0] == 0x010101);
    CHECK(fake.state.flags == (ACCEL_KEY | ACCEL_REPLACE));
    accel_close();
}

/* Off the screen an INDEX target holds codes: a colour's red its index as the 565 word index
   times &H0101, a texel's palette their codes, read back as their low byte. A blend is drawn
   where its alpha is at least a half, as itself. */
TEST(accel_player_codes_indices_off_the_screen)
{
    uint8_t out[4];

    opened();
    indexed_begun(1, 1);

    const uint32_t from = list.n;

    box_at(SOLID, 0, 0, 0, 1, 1, 0, 0, 5, 255);
    CHECK(!played(OTHER, from, false) && fake.last[0].colour == 0xFF00A028);

    list.n = list.nv = list.ni = 0;
    box(TEXTURED, INDEXED, 0, 0, 1, 1, 0, 1);
    box_at(MIXED, 0, 0, 0, 1, 1, 0, 0, 7, 127);
    CHECK(!played(OTHER, 0, false) && calls_of('V') == 2 && fake.palette[0] == 0x002008);

    list.n = list.nv = list.ni = 0;
    box_at(MIXED, 0, 0, 0, 1, 1, 0, 0, 7, 128);
    CHECK(!played(OTHER, 0, false) && calls_of('V') == 3 && fake.state.blend == ACCEL_NONE);

    accel_read(OTHER, 0, 0, 1, 1, out);
    CHECK(out[0] == 0x08 && out[1] == 0x08 && out[2] == 0x08 && out[3] == 0x08);
    accel_close();
}

/* The screen read back: each pixel the index of COLOURS nearest its colour. */
TEST(accel_player_reads_the_shown_index_texture_as_the_nearest_indices)
{
    uint8_t out[4];

    opened();
    indexed_begun(1, 1);

    const uint32_t from = list.n;

    box_at(SOLID, 0, 0, 0, 1, 1, 0, 0, 3, 255);
    CHECK(played(SHOWN, from, true) && fake.last[0].colour == 0xFF424242);

    accel_read(SHOWN, 0, 0, 1, 1, out);
    CHECK(out[0] == 3 && out[3] == 3);
    accel_close();
}
