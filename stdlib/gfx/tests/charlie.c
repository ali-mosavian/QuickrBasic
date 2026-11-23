/* A frame of CHARLIE (charlie.h). */
#include "charlie.h"
#include <stdio.h>

enum {
    BACKDROP = 0,
    HILLS = 1,
    TITLE = 7,
    LEDGE = 28,
    CHARLIE = 34,
    BONE = 126,
    TREE = 162,
    WHITE = 1,
    GROUND = 184,
};

const CharlieStep charlie_steps[] = {
    {.kind = CHARLIE_SPRITE, .picture = BACKDROP, .flags = GFX_PEN_OPAQUE},
    {.kind = CHARLIE_SPRITE, .picture = HILLS, .y = GROUND - 80},
    {.kind = CHARLIE_TILES, .picture = LEDGE, .y = GROUND, .count = CHARLIE_W / 16},
    {.kind = CHARLIE_SPRITE, .picture = TREE, .x = 200, .y = GROUND - 80},
    {.kind = CHARLIE_SPRITE, .picture = CHARLIE, .x = 60, .y = GROUND - 32},
    {.kind = CHARLIE_SPRITE,
     .picture = CHARLIE,
     .x = 100,
     .y = GROUND - 32,
     .flags = GFX_DRAW_FLIPX},
    {.kind = CHARLIE_SPRITE, .picture = BONE, .x = 150, .y = GROUND - 24},
    {.kind = CHARLIE_SPRITE, .picture = TITLE, .x = 75, .y = 20},
    {.kind = CHARLIE_TEXT, .x = 8, .y = 8, .colour = WHITE, .text = "THE AMBER DOWNS"},
};

const uint32_t charlie_nsteps = sizeof charlie_steps / sizeof *charlie_steps;

bool charlie_cuts(const char *const path, CharlieCut *const cuts, const uint32_t n)
{
    FILE *const f = fopen(path, "r");

    if (!f)
        return false;

    memset(cuts, 0, n * sizeof *cuts);
    char line[256];

    while (fgets(line, sizeof line, f)) {
        uint32_t i;
        CharlieCut c;

        if (sscanf(
                line,
                " views(%u) = surface.view(sheet, %d, %d, %d, %d)",
                &i,
                &c.x,
                &c.y,
                &c.w,
                &c.h
            ) == 5 &&
            i < n)
            cuts[i] = c;
    }

    fclose(f);
    return true;
}

#define TRY(call)                                                                                  \
    do {                                                                                           \
        const Called done_ = (call);                                                               \
        if (done_.error)                                                                           \
            return done_.error;                                                                    \
    } while (0)

/* Step s drawn with its view. */
static uint8_t step(
    const QxHost *const host,
    const CharlieStep *const s,
    const CharlieCut *const cut,
    const int32_t view
)
{
    switch (s->kind) {
    case CHARLIE_SPRITE:
        TRY(CALL_ON(host, "draw.sprite", I(view), F(s->x), F(s->y), I(s->flags)));
        break;

    case CHARLIE_TILES: {
        int32_t cells[CHARLIE_W];

        for (int32_t i = 0; i < s->count; i++)
            cells[i] = view;

        QxArray map = gfx_array(cells, (uint32_t)s->count, sizeof *cells);
        TRY(CALL_ON(
            host,
            "draw.tiles",
            ARRAY(&map),
            I(s->count),
            F(s->x),
            F(s->y),
            I(cut->w),
            I(cut->h),
            I(s->flags)
        ));
        break;
    }
    case CHARLIE_TEXT:
        TRY(CALL_ON(
            host,
            "draw.text",
            F(s->x),
            F(s->y),
            ((QxArg){.s = {s->text, (uint32_t)strlen(s->text)}}),
            I(s->colour)
        ));

        break;
    }

    return 0;
}

uint8_t charlie_draw(const QxHost *const host, const CharlieCut *const cuts, int32_t *const views)
{
    const Called sheet = CALL_ON(host, "surface.load", STR(CHARLIE_ATLAS));

    if (sheet.error)
        return sheet.error;

    TRY(CALL_ON(host, "display.open", I(CHARLIE_W), I(CHARLIE_H), I(8), I(0)));
    TRY(CALL_ON(host, "display.palette_of", I(0), I(sheet.result.i)));

    for (uint32_t i = 0; i < charlie_nsteps; i++) {
        const CharlieStep *const s = &charlie_steps[i];
        const CharlieCut *const c = &cuts[s->picture];
        views[i] = 0;

        if (s->kind != CHARLIE_TEXT) {
            const Called v = CALL_ON(
                host,
                "surface.view",
                I(sheet.result.i),
                I(c->x),
                I(c->y),
                I(c->w),
                I(c->h)
            );

            if (v.error)
                return v.error;

            views[i] = v.result.i;
        }

        const uint8_t error = step(host, s, c, views[i]);

        if (error)
            return error;
    }

    return 0;
}
