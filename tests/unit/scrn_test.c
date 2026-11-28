/* The editor's screen (ui/screen.c): what is drawn stays on it. */
#include "plat/host/host.h"
#include "ui/screen.h"
#include "ui/theme.h"
#include "unit.h"

/* Text past a row's right end is cut, never carried onto the next row. On the last row it ran past
   the cells: Display Size's help line overwrote the next heap block, and choosing 132 x 43 hung. */
TEST(scrn_cuts_text_at_the_right_edge)
{
    CHECK(scrn_size(PSCR_80X25));
    scrn_fill(3, 0, SL_ED, ' ', 80);
    scrn_fill(4, 0, SL_ED, ' ', 80);
    scrn_str(3, 76, "ABCDEFGH", SL_ED, SL_ED);
    scrn_strn(4, 78, (const uint8_t *)"xyz", 3, SL_ED);
    scrn_fill(24, 79, SL_ED, '#', 4);
    scrn_flip();

    CHECK((host_cell(0, 3, 79) & 0xFF) == 'D');
    CHECK((host_cell(0, 4, 0) & 0xFF) == ' ');
    CHECK((host_cell(0, 4, 79) & 0xFF) == 'y');
    CHECK((host_cell(0, 5, 0) & 0xFF) != 'z');
}
