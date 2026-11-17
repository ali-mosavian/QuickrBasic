/* The editor's page (page 0) in the terminal: its cells in CP437's glyphs as UTF-8, its 16 colours
 * as 24-bit ones, on the terminal's alternate screen; each draw writes only the cells that changed,
 * all of them after a change of size or colours. The screen is the terminal's size (host_text_fit),
 * 80 by 25 at least. */
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "plat/host/host.h"
#include "sdl.h"

enum { PAGE_EDIT = 0, OUT_CAP = 1 << 20, ROWS_MIN = 25 };

/* CP437 past ASCII's printable half: 00h..1Fh, 7Fh..FFh. */
static const char *const low = " ☺☻♥♦♣♠•◘○◙♂♀♪♫☼►◄↕‼¶§▬↨↑↓→←∟↔▲▼";
static const char *const high =
    "⌂ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒáíóúñÑªº¿⌐¬½¼¡«»░▒▓│┤╡╢╖╕╣║╗╝╜╛┐└┴┬├─┼╞╟╚╔╩╦╠═╬╧╨╤╥╙╘╒╓╫╪┘┌"
    "█▄▌▐▀αßΓπΣσµτΦΘΩδ∞φε∩≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ ";

static struct {
    uint8_t bytes[256]; /* each code's UTF-8's length, in utf8 */
    char utf8[256 * 4];
    PscrCell last[PSCR_ROWS_MAX * PSCR_COLS_MAX];
    uint8_t rows, cols, palette[48];
    uint32_t size;  /* the terminal's, as host_text_fit was given it */
    int32_t cursor; /* where the terminal's is, row << 8 | col; -1 hidden */
    char out[OUT_CAP];
    uint32_t n;
} ansi;

static void glyphs(const char *s, const uint32_t first, const uint32_t count)
{
    for (uint32_t c = first; c < first + count; c++) {
        const uint8_t lead = (uint8_t)*s;
        const uint8_t n = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
        ansi.bytes[c] = n;

        for (uint32_t i = 0; i < n; i++)
            ansi.utf8[c * 4 + i] = *s++;
    }
}

static void put(const char *const s, const uint32_t n)
{
    for (uint32_t i = 0; i < n && ansi.n < OUT_CAP; i++)
        ansi.out[ansi.n++] = s[i];
}

static void puts_(const char *const s)
{
    uint32_t n = 0;

    while (s[n])
        n++;

    put(s, n);
}

static void num(uint32_t v)
{
    char d[10];
    uint32_t n = 0;

    do
        d[n++] = (char)('0' + v % 10);
    while (v /= 10);

    while (n)
        put(&d[--n], 1);
}

static void flush(void)
{
    for (uint32_t at = 0; at < ansi.n;) {
        const ssize_t w = write(STDOUT_FILENO, ansi.out + at, ansi.n - at);

        if (w <= 0)
            break;

        at += (uint32_t)w;
    }

    ansi.n = 0;
}

static void colour(const char *const which, const uint8_t i)
{
    const uint8_t *const p = ansi.palette + i * 3;
    puts_(which);

    for (uint32_t c = 0; c < 3; c++) {
        puts_(";");
        num((uint32_t)(p[c] << 2 | p[c] >> 4));
    }
}

static void restore(void)
{
    puts_("\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l");
    flush();
}

/* The terminal's size, within what the screen can be, to pscr_text when it changed. */
static void fit(void)
{
    struct winsize w;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) || !w.ws_row || !w.ws_col)
        return;

    const uint32_t rows = w.ws_row < ROWS_MIN        ? ROWS_MIN
                          : w.ws_row > PSCR_ROWS_MAX ? PSCR_ROWS_MAX
                                                     : w.ws_row;
    const uint32_t cols = w.ws_col < PSCR_COLS       ? PSCR_COLS
                          : w.ws_col > PSCR_COLS_MAX ? PSCR_COLS_MAX
                                                     : w.ws_col;

    if ((rows << 8 | cols) != ansi.size) {
        ansi.size = rows << 8 | cols;
        host_text_fit((uint8_t)rows, (uint8_t)cols);
    }
}

void sdl_ansi_open(void)
{
    fit();
    glyphs(low, 0, 32);

    for (uint32_t c = 32; c < 127; c++) {
        ansi.utf8[c * 4] = (char)c;
        ansi.bytes[c] = 1;
    }

    glyphs(high, 127, 129);
    puts_("\x1b[?1049h\x1b[?7l\x1b[2J");
    flush();
    atexit(restore);
}

void sdl_ansi_draw(void)
{
    fit();
    const HostScreen s = host_screen_of(PAGE_EDIT);
    const bool all = s.rows != ansi.rows || s.cols != ansi.cols ||
                     __builtin_memcmp(s.palette, ansi.palette, sizeof ansi.palette);

    if (all) {
        ansi.rows = s.rows;
        ansi.cols = s.cols;
        __builtin_memcpy(ansi.palette, s.palette, sizeof ansi.palette);
        puts_("\x1b[2J");
    }

    int32_t attr = -1;

    for (uint32_t row = 0; row < s.rows; row++) {
        bool placed = false;

        for (uint32_t col = 0; col < s.cols; col++) {
            const uint32_t i = row * s.cols + col;
            const PscrCell c = s.cells[i];

            if (!all && c == ansi.last[i]) {
                placed = false;
                continue;
            }

            ansi.last[i] = c;

            if (!placed) {
                puts_("\x1b[");
                num(row + 1);
                puts_(";");
                num(col + 1);
                puts_("H");
                placed = true;
            }

            const uint8_t a = (uint8_t)(c >> 8);

            if (a != attr) {
                attr = a;
                colour("\x1b[38;2", a & 15);
                colour(";48;2", a >> 4 & 7);
                puts_("m");
            }

            const uint8_t code = (uint8_t)c;
            put(ansi.utf8 + code * 4, ansi.bytes[code]);
        }
    }

    const bool on = host_screen().page == PAGE_EDIT && !(s.shape & PSCR_HIDDEN) && s.row < s.rows;
    const int32_t cursor = on ? s.row << 8 | s.col : -1;

    if (ansi.n || cursor != ansi.cursor) {
        ansi.cursor = cursor;

        if (on) {
            puts_("\x1b[");
            num(s.row + 1u);
            puts_(";");
            num(s.col + 1u);
            puts_("H\x1b[?25h");
        } else
            puts_("\x1b[?25l");
    }

    flush();
}
