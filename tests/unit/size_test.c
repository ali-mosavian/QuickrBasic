/* Display Size: the card's text sizes, VESA's past 80 x 50. */
#include "edfix.h"
#include "plat/host/host.h"
#include "ui/mouse.h"
#include "ui/theme.h"
#include "ui/uimain.h"
#include "unit.h"

enum : uint16_t { ALT_O = 0x1800, KEY_Z = 0x2C7A, KEY_1 = 0x0231, KEY_7 = 0x0837 };

/* llrm's headless device (dev/headless/headless.h). */
void headless_set_stdout_is_screen(int yes);

static const char *const files[] = {"main.bas", "gfx.bas", nullptr};

/* Options, Display Size, the item key chooses, as the keys choose it; the screen shown. */
static void choose(Session *const S, const uint16_t key)
{
    host_key_push(KEY_Z);
    host_key_push(key);
    fx_key(S, ALT_O, 0);
    fx_shown(S);
}

/* Screen row row's text from column col, or "" past its end. */
static const char *from(const uint8_t row, const uint8_t col)
{
    const char *const s = fx_row(row);
    return strlen(s) > col ? s + col : "";
}

/* 132 x 50 from the menu, the host's card an SVGA's: the screen 132 columns wide, the edit
   window's frame, Help on the menu bar and the cursor's place on the status bar at its right
   edge, as at 80, a click past column 80 in the window; 80 x 25 again after. There was no size
   past 80 x 50. */
TEST(display_size_sets_132_columns)
{
    fx_open_program(S, "print 1\r\n");
    uimn_size(PSCR_80X25);
    them_qbasic();
    fx_shown(S);
    const PscrCell corner = host_cell(0, 1, 79), arrow = host_cell(0, 2, 79);
    CHECK(!strcmp(from(0, 74), "Help") && !strcmp(from(24, 70), "00001:001"));

    CHECK(uimn.nsizes == 8);

    if (uimn.nsizes < 7) { /* no 7th item: the menu would wait for one */
        fx_done(S, files);
        return;
    }

    choose(S, KEY_7);
    const HostScreen s = host_screen();
    CHECK(s.cols == 132 && s.rows == 50);
    CHECK(!strcmp(from(0, 126), "Help") && !strcmp(from(49, 122), "00001:001"));
    CHECK(host_cell(0, 1, 131) == corner && host_cell(0, 2, 131) == arrow);
    uimn.mouse = (UiMouse){.event = MS_PRESS, .row = uimn.ed.row0, .col = 100};

    uimn_key(S, KEY_MOUSE);
    CHECK(uimn.edline.col == 100 - uimn.ed.x - 1);

    choose(S, KEY_1);
    CHECK(host_screen().cols == 80 && host_screen().rows == 25 && host_cell(0, 1, 79) == corner);
    fx_done(S, files);
}

/* A run ending in SCREEN 13 under the IDE's 80 x 50: Press any key on the output's last row,
   the 25th, and the editor back. It was put on the IDE's 50th: Illegal function call on DOS, a
   crash here. */
TEST(press_any_key_on_the_outputs_last_row)
{
    fx_open_program(S, "sub hi\r\nscreen 13\r\nend sub\r\n");
    uimn_size((PscrSize){80, 50});

    host_key_push(' ');
    headless_set_stdout_is_screen(1);
    uimn_run(S);
    headless_set_stdout_is_screen(0);
    fx_shown(S);
    CHECK(host_screen().rows == 50 && strstr(fx_row(49), "<F5=Run>"));
    uimn_size(PSCR_80X25);
    fx_done(S, files);
}
