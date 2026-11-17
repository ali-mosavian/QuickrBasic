/* A page of the graphics screen in memory, a byte a pixel (pixmem.h). */
#include <string.h>

#include "pixmem.h"

enum { PLANES = 4, CELL_WIDTH = 8 };

static u8 *pixels;
static unsigned width, height, kind, mask;
static void (*touch)(unsigned top, unsigned count);

void pm_use(u8 *to, unsigned w, unsigned h, unsigned k, void (*told)(unsigned top, unsigned count))
{
    pixels = to;
    width = w;
    height = h;
    kind = k;
    mask = k == PM_LINEAR ? 0xFF : k == PM_PACKED4 ? 3 : k == PM_PACKED2 ? 1 : 0x0F;
    touch = told;
}

static u8 *pixel_at(unsigned x, unsigned y)
{
    return pixels + (unsigned long)y * width + x;
}

static unsigned combined(unsigned old, unsigned color, unsigned operation)
{
    switch (operation & 3) {
    case 1:
        return old & color;
    case 2:
        return old | color;
    case 3:
        return old ^ color;
    default:
        return color;
    }
}

static void blend(unsigned x, unsigned y, unsigned color, unsigned operation)
{
    u8 *at = pixel_at(x, y);

    *at = (u8)(combined(*at, color & mask, operation) & mask);
}

void pm_plot(unsigned x, unsigned y, unsigned color, unsigned operation)
{
    if (x < width && y < height) {
        blend(x, y, color, operation);
        touch(y, 1);
    }
}

unsigned pm_read(unsigned x, unsigned y)
{
    return x < width && y < height ? *pixel_at(x, y) : 0;
}

int pm_search(int x, int last, unsigned y, unsigned c1, unsigned c2, int match)
{
    int step = last >= x ? 1 : -1, at;

    for (at = x;; at += step) {
        unsigned value = *pixel_at((unsigned)at, y);
        int wanted = value == c1 || value == c2;

        if (wanted == (match != 0))
            return at;
        if (at == last)
            return -1;
    }
}

int pm_run(int x, int last, unsigned y, unsigned edge, unsigned paint, int limit, int *left, int *right)
{
    int open = pm_search(x, last, y, edge, paint, 0), l, r;

    if (open < 0)
        return 0;
    l = pm_search(open, 0, y, edge, edge, 1);
    r = pm_search(open, limit, y, edge, edge, 1);
    *left = l < 0 ? 0 : l + 1;
    *right = r < 0 ? limit : r - 1;
    return 1;
}

void pm_move_rows(unsigned to, unsigned from, unsigned count)
{
    memmove(pixel_at(0, to), pixel_at(0, from), (unsigned long)count * width);
    touch(to, count);
}

void pm_glyph(unsigned x, unsigned y, const u8 *bits, unsigned rows, unsigned foreground)
{
    unsigned line, bit;

    for (line = 0; line < rows; line++)
        for (bit = 0; bit < CELL_WIDTH; bit++)
            blend(x + bit, y + line, bits[line] & 0x80 >> bit ? foreground : 0, 0);
    touch(y, rows);
}

/* Fills: one box, dot, line and octet for every kind of mode, over blend(). */
static void pm_box(const GdFill *fill, unsigned x, unsigned y, unsigned count, unsigned rows)
{
    unsigned row, column;

    for (row = 0; row < rows; row++)
        for (column = 0; column < count; column++)
            blend(x + column, y + row, fill->color, fill->operation);
    touch(y, rows);
}

static void pm_dot(const GdFill *fill, unsigned x, unsigned y)
{
    blend(x, y, fill->color, fill->operation);
    touch(y, 1);
}

/* Bresenham's line as gdline.asm runs it: `count` pixels along the longer side, the decision value stepping the
   other when it is not negative; the pixels of `style`, its high bit first, are drawn. */
static void pm_line(const GdFill *fill, unsigned x, unsigned y, unsigned dx, unsigned dy, int step_y)
{
    int major = (int)(dx > dy ? dx : dy), minor = (int)(dx > dy ? dy : dx);
    int decision = 4 * minor - major, minor4 = 4 * minor, step4 = 4 * (minor - major);
    int x_major = dx == 0 ? 2 : dx > dy;
    int px = (int)x, py = (int)y, top = py, bottom = py;
    unsigned style = fill->style & 0xFFFF, count = (unsigned)major + 1;

    for (; count; count--) {
        unsigned carry = style >> 15 & 1;

        style = (style << 1 | carry) & 0xFFFF;
        if (carry)
            blend((unsigned)px, (unsigned)py, fill->color, fill->operation);
        if (x_major == 2) {
            py += step_y;
        } else if (x_major == 1) {
            px++;
            if (decision >= 0)
                decision += step4, py += step_y;
            else
                decision += minor4;
        } else {
            py += step_y;
            if (decision >= 0)
                decision += step4, px++;
            else
                decision += minor4;
        }
        top = py < top ? py : top;
        bottom = py > bottom ? py : bottom;
    }
    touch((unsigned)top, (unsigned)(bottom - top + 1));
}

static void pm_octet(const GdFill *fill, unsigned cx, unsigned cy, int u, int v, int w, int z)
{
    unsigned color = fill->color;

    blend(cx + (unsigned)u, cy - (unsigned)v, color, 0);
    blend(cx - (unsigned)u, cy - (unsigned)v, color, 0);
    blend(cx + (unsigned)u, cy + (unsigned)v, color, 0);
    blend(cx - (unsigned)u, cy + (unsigned)v, color, 0);
    blend(cx + (unsigned)w, cy - (unsigned)z, color, 0);
    blend(cx - (unsigned)w, cy - (unsigned)z, color, 0);
    blend(cx + (unsigned)w, cy + (unsigned)z, color, 0);
    blend(cx - (unsigned)w, cy + (unsigned)z, color, 0);
    touch(cy - (unsigned)v, 1);
    touch(cy + (unsigned)v, 1);
    touch(cy - (unsigned)z, 1);
    touch(cy + (unsigned)z, 1);
}

void pm_fill_select(GdFill *fill, unsigned color, unsigned operation)
{
    unsigned byte = color & mask;

    operation &= 3;
    fill->box = pm_box;
    fill->dot = pm_dot;
    fill->line = pm_line;
    fill->octet = pm_octet;
    fill->color = color & 0xFF;
    fill->style = 0xFFFF;
    fill->operation = operation;
    fill->keep = operation == 0 ? 0 : operation == 1 ? byte : operation == 2 ? ~byte & 0xFF : 0xFF;
    fill->flip = operation == 1 ? 0 : byte;
}

/* GET and PUT. A row of the array is one run of bits for each plane (the 16-colour modes: bit n of a pixel's colour
   in plane n), or the packed pixels of CGA, or a byte each; the leftmost pixel is the high bit, and the last byte's
   unused bits are zero. */
static unsigned bits_of(void)
{
    return kind == PM_PACKED4 ? 2 : 1;
}

static unsigned planes_of(void)
{
    return kind == PM_PLANAR ? PLANES : 1;
}

void pm_get(unsigned x, unsigned y, unsigned box_width, unsigned rows, u8 *out)
{
    unsigned row, column, plane;

    if (kind == PM_LINEAR) {
        for (row = 0; row < rows; row++, out += box_width)
            memcpy(out, pixel_at(x, y + row), box_width);
        return;
    }
    {
        unsigned bits = bits_of(), planes = planes_of(), bytes = (box_width * bits + 7) >> 3;

        for (row = 0; row < rows; row++, out += planes * bytes) {
            memset(out, 0, planes * bytes);
            for (column = 0; column < box_width; column++) {
                unsigned value = *pixel_at(x + column, y + row);

                for (plane = 0; plane < planes; plane++) {
                    unsigned at = column * bits, piece = kind == PM_PLANAR ? value >> plane & 1 : value;

                    out[plane * bytes + (at >> 3)] |= (u8)(piece << (8 - bits - (at & 7)));
                }
            }
        }
    }
}

void pm_put(unsigned x, unsigned y, unsigned box_width, unsigned rows, const u8 *in, unsigned operation,
            unsigned invert)
{
    unsigned row, column, plane;
    unsigned bits = kind == PM_LINEAR ? 8 : bits_of(), planes = planes_of(), bytes = (box_width * bits + 7) >> 3;

    for (row = 0; row < rows; row++, in += planes * bytes) {
        for (column = 0; column < box_width; column++) {
            unsigned value = 0, at = column * bits;

            for (plane = 0; plane < planes; plane++) {
                unsigned piece = in[plane * bytes + (at >> 3)] >> (8 - bits - (at & 7)) & ((1u << bits) - 1);

                value |= kind == PM_PLANAR ? piece << plane : piece;
            }
            if (invert)
                value ^= mask;
            blend(x + column, y + row, value, invert ? 0 : operation);
        }
        touch(y + row, 1);
    }
}
