/* Fonts: a sheet of glyphs cut into cells, and the built-in 8 by 8 as one.
 *
 * # How it works
 * A font names its sheet by handle, its cells w by h, row by row from its first code. The built-in
 * is the runtime's 8 by 8 font drawn into a sheet of its own on first use, 16 glyphs a row from
 * code 0, index 1 where a glyph's bit is set. Text draws each glyph as a picture, so a turned
 * camera turns it.
 */
#ifndef GFX_FONT_H
#define GFX_FONT_H
#include "handle.h"
#include "pixels.h"

typedef struct {
    uint32_t sheet; /* 0 the built-in's */
    int32_t w, h, columns;
    uint32_t first, count;
} Font;

typedef struct {
    HdlTable table;
    Font builtin;
    uint8_t *pixels; /* the built-in's sheet */
} Fonts;

void font_init(Fonts *fs);
void font_done(Fonts *fs);
/* A font of w by h cells of sheet, which is sheet_w by sheet_h: its handle, 0 no memory. */
uint32_t font_new(
    Fonts *fs,
    uint32_t sheet,
    int32_t sheet_w,
    int32_t sheet_h,
    int32_t w,
    int32_t h
);
/* h's font, 0 the built-in: null, none. */
const Font *font_get(const Fonts *fs, uint32_t h);
/* The built-in's sheet into *out: false, no memory. */
bool font_builtin(Fonts *fs, Pixels *out);
/* code's cell in f's sheet: false, none. */
bool font_glyph(const Font *f, uint8_t code, Rect *cell);
#endif
