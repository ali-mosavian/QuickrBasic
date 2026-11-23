/* The screen as RGBA (view.h).
 *
 * # How it works
 * A display's thread calls it while the program runs: it reads the screen as a VGA card does. A
 * graphics mode is the runtime's framebuffer (the host video's), unless the editor keeps it; text is
 * a pscr page in the VGA's font, attribute bit 7 blinking and the cursor shown in the blink's on
 * phase.
 *
 * Every row count takes the 16-line font. A VGA's 400 lines held 43 and 50 rows only in the 8-line
 * one, its strokes doubled and its rows touching; a display has the lines, so the frame grows.
 */
#include "plat/view/view.h"
#include "plat/host/host.h"
#include "qbrt/qbrt.h"

/* A text cell is the VGA's 9 dots wide: the font's 8, the 9th the 8th again for C0h..DFh (lines that
   join), else the background. */
enum { CELL_W = VIEW_CELL_W, CELL_H = VIEW_CELL_H, FONT_W = 8 };
enum : uint8_t {
    POINTER = 0x77
}; /* the mouse's text cursor: the cell's colours inverted, as INT 33h's */

unsigned host_width(void);
unsigned host_height(void);
const uint8_t *host_pixels(void);
void host_colors(uint8_t rgb[256][3]);
extern const unsigned char qb_vga_font_16[256 * CELL_H];

static_assert(PSCR_ROWS_MAX * CELL_H <= VIEW_MAX_H);

bool view_text_shown;
static ViewFrame out;

static uint32_t rgba(const uint8_t r, const uint8_t g, const uint8_t b)
{
    return 0xFF000000u | (uint32_t)b << 16 | (uint32_t)g << 8 | r;
}

static uint8_t eight(const uint8_t six) { return (uint8_t)(six << 2 | six >> 4); }

static void graphics(void)
{
    uint8_t rgb[256][3];
    host_colors(rgb);
    uint32_t colours[256];

    for (uint32_t i = 0; i < 256; i++)
        colours[i] = rgba(rgb[i][0], rgb[i][1], rgb[i][2]);

    const uint8_t *const px = host_pixels();
    out.width = host_width();
    out.height = host_height();
    out.cell_h = 0;

    for (uint32_t i = 0; i < out.width * out.height; i++)
        out.rgba[i] = colours[px[i]];
}

/* A CGA cursor line, 0-7, in a VGA cell. */
static uint32_t vga_line(const uint32_t line) { return line < 4 ? line : line + CELL_H - 9; }

static void text(const bool blink, const ViewPointer pointer)
{
    const HostScreen s = host_screen();
    uint32_t colours[16];

    for (uint32_t i = 0; i < 16; i++)
        colours[i] =
            rgba(eight(s.palette[i * 3]), eight(s.palette[i * 3 + 1]), eight(s.palette[i * 3 + 2]));

    out.width = s.cols * CELL_W;
    out.height = s.rows * CELL_H;
    out.cell_h = CELL_H;

    for (uint32_t row = 0; row < s.rows; row++)
        for (uint32_t col = 0; col < s.cols; col++) {
            const PscrCell c = s.cells[row * s.cols + col];
            const bool under = pointer.shown && row == pointer.row && col == pointer.col;
            const uint8_t attr = (uint8_t)(c >> 8) ^ (under ? POINTER : 0);
            const uint32_t bg = colours[attr >> 4 & 7];
            const uint32_t fg = attr & 0x80 && !blink ? bg : colours[attr & 15];
            const uint8_t code = (uint8_t)c;
            const unsigned char *const glyph = qb_vga_font_16 + code * CELL_H;
            const bool joins = code >= 0xC0 && code <= 0xDF;
            uint32_t *p = out.rgba + row * CELL_H * out.width + col * CELL_W;

            for (uint32_t line = 0; line < CELL_H; line++, p += out.width) {
                for (uint32_t bit = 0; bit < FONT_W; bit++)
                    p[bit] = glyph[line] & 0x80 >> bit ? fg : bg;

                p[FONT_W] = joins && glyph[line] & 1 ? fg : bg;
            }
        }

    if (!blink || s.shape & PSCR_HIDDEN || s.row >= s.rows || s.col >= s.cols)
        return;

    /* the shape's lines are a CGA cell's, of 8: the VGA BIOS's emulation keeps lines 0-3 from the
       top, 4-7 from the bottom, its last line left clear (0607h: 13-14 of 16) */
    const uint32_t first = vga_line(s.shape >> 8 & 7), last = vga_line(s.shape & 7) + 1;
    const uint32_t fg = colours[s.cells[s.row * s.cols + s.col] >> 8 & 15];

    for (uint32_t line = first; line < last && line < CELL_H; line++)
        for (uint32_t bit = 0; bit < CELL_W; bit++)
            out.rgba[(s.row * CELL_H + line) * out.width + s.col * CELL_W + bit] = fg;
}

const ViewFrame *view_frame(const bool blink, const ViewPointer pointer)
{
    if (qbrt_graphics() && !view_text_shown)
        graphics();
    else
        text(blink, pointer);

    return &out;
}
