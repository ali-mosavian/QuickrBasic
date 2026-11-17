/* The screen as RGBA (view.h).
 *
 * # How it works
 * A display's thread calls it while the program runs: it reads the screen as a VGA card does. A
 * graphics mode is the runtime's framebuffer (the host video's), unless the editor keeps it; text is
 * a pscr page in the VGA's font, attribute bit 7 blinking and the cursor shown in the blink's on
 * phase.
 */
#include "plat/view/view.h"
#include "plat/host/host.h"
#include "qbrt/qbrt.h"

/* A text cell is the VGA's 9 dots wide: the font's 8, the 9th the 8th again for C0h..DFh (lines that
   join), else the background. */
enum { CELL_W = VIEW_CELL_W, FONT_W = 8, PAGE_OUT = 1 };
enum : uint8_t {
    POINTER = 0x77
}; /* the mouse's text cursor: the cell's colours inverted, as INT 33h's */

unsigned host_width(void);
unsigned host_height(void);
const uint8_t *host_pixels(void);
void host_colors(uint8_t rgb[256][3]);
extern const unsigned char qb_vga_font_8[256 * 8], qb_vga_font_16[256 * 16];

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

static void text(const HostScreen s, const bool blink, const bool cursor, const ViewPointer pointer)
{
    const uint32_t ch = s.rows == 25 ? 16 : 8;
    const unsigned char *const font = ch == 16 ? qb_vga_font_16 : qb_vga_font_8;
    uint32_t colours[16];

    for (uint32_t i = 0; i < 16; i++)
        colours[i] =
            rgba(eight(s.palette[i * 3]), eight(s.palette[i * 3 + 1]), eight(s.palette[i * 3 + 2]));

    out.width = PSCR_COLS * CELL_W;
    out.height = s.rows * ch;
    out.cell_h = ch;

    for (uint32_t row = 0; row < s.rows; row++)
        for (uint32_t col = 0; col < PSCR_COLS; col++) {
            const PscrCell c = s.cells[row * PSCR_COLS + col];
            const bool under = pointer.shown && row == pointer.row && col == pointer.col;
            const uint8_t attr = (uint8_t)(c >> 8) ^ (under ? POINTER : 0);
            const uint32_t bg = colours[attr >> 4 & 7];
            const uint32_t fg = attr & 0x80 && !blink ? bg : colours[attr & 15];
            const uint8_t code = (uint8_t)c;
            const unsigned char *const glyph = font + code * ch;
            const bool joins = code >= 0xC0 && code <= 0xDF;
            uint32_t *p = out.rgba + row * ch * out.width + col * CELL_W;

            for (uint32_t line = 0; line < ch; line++, p += out.width) {
                for (uint32_t bit = 0; bit < FONT_W; bit++)
                    p[bit] = glyph[line] & 0x80 >> bit ? fg : bg;

                p[FONT_W] = joins && glyph[line] & 1 ? fg : bg;
            }
        }

    if (!blink || !cursor || s.shape & PSCR_HIDDEN || s.row >= s.rows || s.col >= PSCR_COLS)
        return;

    /* the shape's lines are a CGA cell's, of 8: the VGA scales them to its cell */
    const uint32_t first = (s.shape >> 8 & 7) * ch / 8, last = ((s.shape & 7) + 1) * ch / 8;
    const uint32_t fg = colours[s.cells[s.row * PSCR_COLS + s.col] >> 8 & 15];

    for (uint32_t line = first; line < last && line < ch; line++)
        for (uint32_t bit = 0; bit < CELL_W; bit++)
            out.rgba[(s.row * ch + line) * out.width + s.col * CELL_W + bit] = fg;
}

const ViewFrame *view_frame(const ViewWhat what, const bool blink, const ViewPointer pointer)
{
    const HostScreen s = host_screen();

    if (qbrt_graphics() && (what == VIEW_OUTPUT || !view_text_shown))
        graphics();
    else if (what == VIEW_SCREEN || s.page == PAGE_OUT)
        text(s, blink, true, pointer);
    else /* the editor's page shown: the output's, without its cursor */
        text(host_screen_of(PAGE_OUT), blink, false, pointer);

    return &out;
}
