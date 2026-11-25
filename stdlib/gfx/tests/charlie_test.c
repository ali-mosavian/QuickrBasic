/* A frame of CHARLIE both ways: by its commands drawn, and by its pixels read against the steps
   pasted by hand from the atlas, a loop each. */
#include <stdio.h>
#include <stdlib.h>
#include "charlie.h"
#include "qbrt/llrm/qb/dev/host/vgadata.h"
#include "test.h"

/* The atlas's height is read: fixed, it went stale whenever charlieart.py packed other art. */
enum { ATLAS_W = 1024, GLYPH = 8 };

typedef struct {
    uint8_t px[CHARLIE_H][CHARLIE_W];
} CharlieFrame;

/* w by h of the atlas at sx, sy onto f at x, y: keyed unless opaque, mirrored with flip. */
static void paste(
    CharlieFrame *const f,
    const uint8_t *const atlas,
    const CharlieCut *const c,
    const int32_t x,
    const int32_t y,
    const bool opaque,
    const bool flip
)
{
    for (int32_t j = 0; j < c->h; j++)
        for (int32_t i = 0; i < c->w; i++) {
            const uint8_t texel = atlas[(c->y + j) * ATLAS_W + c->x + (flip ? c->w - 1 - i : i)];
            const int32_t px = x + i, py = y + j;

            if ((texel || opaque) && px >= 0 && py >= 0 && px < CHARLIE_W && py < CHARLIE_H)
                f->px[py][px] = texel;
        }
}

static void by_hand(CharlieFrame *const f, const uint8_t *const atlas, const CharlieCut *const cuts)
{
    memset(f, 0, sizeof *f);

    for (uint32_t n = 0; n < charlie_nsteps; n++) {
        const CharlieStep *const s = &charlie_steps[n];
        const CharlieCut *const c = &cuts[s->sprite];
        const bool opaque = s->flags & GFX_DRAW_OPAQUE, flip = s->flags & GFX_DRAW_FLIP_X;

        if (s->kind == CHARLIE_SPRITE)
            paste(f, atlas, c, s->x, s->y, opaque, flip);

        for (int32_t i = 0; s->kind == CHARLIE_TILES && i < s->count; i++)
            paste(f, atlas, c, s->x + i * c->w, s->y, opaque, flip);

        for (int32_t i = 0; s->kind == CHARLIE_TEXT && s->text[i]; i++)
            for (int32_t j = 0; j < GLYPH; j++)
                for (int32_t b = 0; b < GLYPH; b++)
                    if (qb_vga_font_8[(uint8_t)s->text[i] * GLYPH + j] & 0x80 >> b)
                        f->px[s->y + j][s->x + i * GLYPH + b] = (uint8_t)s->colour;
    }
}

static const int32_t DREW[] = {
    [CHARLIE_SPRITE] = GFX_DISPLAY_DREW_SPRITE,
    [CHARLIE_TILES] = GFX_DISPLAY_DREW_TILES,
    [CHARLIE_TEXT] = GFX_DISPLAY_DREW_TEXT,
};

TEST(gfx_charlie_frame_both_ways)
{
    gfx_fresh(S);
    static CharlieCut cuts[CHARLIE_CUTS];
    int32_t views[16];
    CHECK(charlie_nsteps <= 16 && charlie_cuts(CHARLIE_SHEET, cuts, CHARLIE_CUTS));
    CHECK(!charlie_draw(&S->natv.host, cuts, views));

    CHECK(CALL("display.draws").result.i == (int32_t)charlie_nsteps);

    for (uint32_t n = 0; n < charlie_nsteps; n++) {
        const CharlieStep *const s = &charlie_steps[n];
        gfx_display_command d;
        CHECK(!CALL("display.get_drawn", I(n + 1), REF(&d)).error);
        CHECK(d.kind == DREW[s->kind] && d.x == (float)s->x && d.y == (float)s->y);
        CHECK(d.source == (s->kind == CHARLIE_SPRITE ? views[n] : 0) && d.flags == s->flags);
    }

    CALL("display.flip");
    const int32_t sheet = CALL("surface.load", STR(CHARLIE_ATLAS)).result.i;
    const int32_t rows = CALL("surface.height", I(sheet)).result.i;
    CHECK(CALL("surface.width", I(sheet)).result.i == ATLAS_W && rows > 0);
    uint8_t *const atlas = malloc((size_t)rows * ATLAS_W);
    QxArray a = gfx_array(atlas, (uint32_t)rows * ATLAS_W, 1);
    CHECK(!CALL("surface.read", I(sheet), I(0), I(0), I(ATLAS_W), I(rows), ARRAY(&a)).error);

    static CharlieFrame want, got;
    by_hand(&want, atlas, cuts);
    QxArray g = gfx_array(got.px, sizeof got.px, 1);
    CHECK(!CALL("surface.read", I(SURF_SCREEN), I(0), I(0), I(CHARLIE_W), I(CHARLIE_H), ARRAY(&g))
               .error);

    uint32_t wrong = 0, first = 0;

    for (uint32_t i = 0; i < sizeof got.px; i++)
        if (got.px[i / CHARLIE_W][i % CHARLIE_W] != want.px[i / CHARLIE_W][i % CHARLIE_W] &&
            !wrong++)
            first = i;

    if (wrong)
        printf(
            "  %u pixels differ, the first at %u, %u\n",
            wrong,
            first % CHARLIE_W,
            first / CHARLIE_W
        );

    free(atlas);
    CHECK(!wrong);
}
