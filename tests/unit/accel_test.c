/* plat/dos32/accel/file.c: a driver file's header read, as the loader reads one for each card. */
#include "plat/dos32/accel/accel.h"
#include "plat/dos32/accel/cut.h"
#include "qx/qxlist.h"
#include <stddef.h>
#include "unit.h"
#include <string.h>

/* A driver file of Voodoo 1 and 2's IDs, its image's four bytes after them. */
static const uint8_t VOODOO[] = {
    'Q',  'X',  'D',  1,    ACCEL_ABI, 0,    0,    0,    2,   0,   0,   0,
    0x01, 0x00, 0x1A, 0x12, 0x02,      0x00, 0x1A, 0x12, 'Q', 'X', 'I', 1,
};

/* A header names the cards it drives by vendor and device: a Banshee's is not among them. */
TEST(accel_driver_files_name_their_cards)
{
    const PciFunction one = {.vendor_id = 0x121A, .device_id = 0x0001};

    const PciFunction two = {.vendor_id = 0x121A, .device_id = 0x0002};

    const PciFunction banshee = {.vendor_id = 0x121A, .device_id = 0x0003};

    CHECK(accel_drives(VOODOO, sizeof VOODOO, &one) && accel_drives(VOODOO, sizeof VOODOO, &two));
    CHECK(!accel_drives(VOODOO, sizeof VOODOO, &banshee));
}

/* A header read short names only the IDs it holds; another magic or ABI names none. */
TEST(accel_driver_files_are_read_whole_or_not_at_all)
{
    const PciFunction two = {.vendor_id = 0x121A, .device_id = 0x0002};

    uint8_t other[sizeof VOODOO];

    CHECK(!accel_drives(VOODOO, ACCEL_HEAD + 4, &two));
    memcpy(other, VOODOO, sizeof other);
    other[4] = ACCEL_ABI + 1;
    CHECK(!accel_drives(other, sizeof other, &two));
    other[4] = ACCEL_ABI;
    other[3] = 2;
    CHECK(!accel_drives(other, sizeof other, &two));
}

/* The image follows the IDs; a file whose IDs run past it has none. */
TEST(accel_driver_files_hold_their_image_after_the_ids)
{
    const uint8_t *image;
    uint32_t n;

    CHECK(accel_image(VOODOO, sizeof VOODOO, &image, &n) && n == 4 && !memcmp(image, "QXI", 3));
    CHECK(!accel_image(VOODOO, ACCEL_HEAD + 8, &image, &n));
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

/* ---- the player against a driver that records its calls (player.c) ---- */

enum { SCREEN_W = 320, SCREEN_H = 200, SLOTS = 8, TILE = 256 * 32, CALLS = 256 };

/* A call a letter: Texture, Upload, Forget, tarGet, Drop, Aim, State, triangles (V), shoW,
   Carry, Read. */
static struct {
    char calls[CALLS];
    uint32_t ncalls;
    uint16_t texture_w[SLOTS], texture_h[SLOTS];
    uint32_t textures;
    uint16_t up[SLOTS][TILE];
    AccelVertex last[3];
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
fake_upload(uint32_t slot, uint16_t y, uint16_t h, const uint16_t *texels, uint32_t pitch)
{
    called('U');

    for (uint32_t r = 0; r < h; r++)
        memcpy(fake.up[slot] + (y + r) * fake.texture_w[slot], texels + r * pitch, pitch * 2);
}

static void ACCEL_CALL fake_forget(uint32_t slot) { (void)slot, called('F'); }

static bool ACCEL_CALL fake_target(uint32_t slot, uint16_t w, uint16_t h, bool depth)
{
    (void)slot, (void)w, (void)h, (void)depth;
    called('G');
    return true;
}

static void ACCEL_CALL fake_drop(uint32_t slot) { (void)slot, called('D'); }

static void ACCEL_CALL fake_aim(uint32_t slot, AccelRect clip)
{
    (void)slot, (void)clip, called('A');
}

static void ACCEL_CALL fake_state(const AccelState *s) { (void)s, called('S'); }

static void ACCEL_CALL fake_triangles(const AccelVertex *v, uint32_t n)
{
    called('V');
    memcpy(fake.last, v + n - 3, sizeof fake.last);
}

static void ACCEL_CALL fake_show(bool vsync) { (void)vsync, called('W'); }

static void ACCEL_CALL fake_carry(void) { called('C'); }

static void ACCEL_CALL fake_read(uint32_t slot, AccelRect r, uint8_t *out)
{
    (void)slot;
    called('R');
    memset(out, 0x42, (size_t)r.w * r.h * 4);
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
    .triangles = fake_triangles,
    .show = fake_show,
    .carry = fake_carry,
    .read = fake_read,
};

/* A vertex as the FIXED pipeline's attributes find it. */
typedef struct {
    float x, y, z, w, u, v;
    uint16_t rx, ry, rw, rh;
    uint8_t r, g, b, a;
} Vertex;

enum { SHOWN = 100, ART = 7, SOLID = 1, TEXTURED = 2, VERTICES = 1, INDICES = 2, LIST = 1024 };

static struct {
    uint32_t words[LIST], n;
    Vertex v[64];
    uint32_t nv, index[96], ni;
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

static void pipeline(const uint32_t id, const QxFixedSource source, const uint32_t flags)
{
    PUT(QX_LIST_PIPELINE,
        id,
        QX_BLEND_NONE,
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

/* A new list: the screen's texture, two pipelines, the art's texture w by h. */
static void begun(const uint32_t w, const uint32_t h)
{
    list.n = list.nv = list.ni = 0;
    PUT(QX_LIST_TEXTURE, SHOWN, SCREEN_W, SCREEN_H, QX_TEXTURE_RGBA);
    PUT(QX_LIST_TEXTURE, ART, w, h, QX_TEXTURE_RGBA);
    pipeline(SOLID, QX_FIXED_COLOUR, 0);
    pipeline(TEXTURED, QX_FIXED_TEXEL, 0);
}

/* A quad over pixels x0 to x1, y0 to y1, texels u0 to u1 across, drawn with pipeline from
   texture. */
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
    const uint32_t at = list.nv, first = list.ni;
    const float xs[] = {x0, x1, x1, x0}, ys[] = {y0, y0, y1, y1}, us[] = {u0, u1, u1, u0};

    const uint32_t corners[] = {0, 1, 2, 0, 2, 3};

    for (uint32_t i = 0; i < 4; i++)
        list.v[list.nv++] =
            (Vertex){xs[i] - 0.5f, ys[i] - 0.5f, 0, 1, us[i], 0, .r = 255, .a = 255};

    for (uint32_t i = 0; i < 6; i++)
        list.index[list.ni++] = at + corners[i];

    PUT(QX_LIST_DRAW, pipe, VERTICES, INDICES, first, 6, 255, texture);
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
        QX_LIST_PASS | 2 << QX_LIST_SIZE,
        into,
    };

    memcpy(words, list.words, draws_from * 4);
    n = draws_from;
    memcpy(words + n, head, sizeof head);
    n += sizeof head / 4;
    memcpy(words + n, list.words + draws_from, (list.n - draws_from) * 4);
    n += list.n - draws_from;

    if (show) {
        words[n++] = QX_LIST_SHOW | 2 << QX_LIST_SIZE;
        words[n++] = SHOWN;
    }

    words[n++] = QX_LIST_END | 1 << QX_LIST_SIZE;
    return accel_play(words, n);
}

static void opened(void)
{
    fake = (typeof(fake)){};

    CHECK(accel_open(&FAKE, &(PciFunction){}, SCREEN_W, SCREEN_H, true));
}

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
   second repeating the texture's last column and row past them. */
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
    CHECK(fake.up[2][43] == 31 && fake.up[2][255] == 31 && fake.up[2][42] == 0);
    CHECK(fake.up[2][31 * 256 + 255] == (31 << 5 | 31));
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
