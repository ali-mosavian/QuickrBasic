/* The editor's and the output's pages: page 0 the editor's, page 1 the program's output.
 *
 * # How it works
 * A program in a graphics mode has the whole screen for its output. When the editor is shown, the
 * screen is kept (qbrt_shot_take) in an arena block and the screen put in text mode; the runtime's
 * state stays as the program left it, so a continue, a step or an immediate line draws on. The
 * output shown again sets the mode back and the screen with it (qbrt_shot_show). A run starts in
 * text mode.
 *
 * # asm structure (src/ui/ui.asm)
 * | asm | here | |
 * |---|---|---|
 * | ui_in, ui_out, ui_outclr | upag_in, upag_out, upag_clear | MIRROR |
 * | (none) | upag_init, upag_output, the kept screen | NEW |
 */
#include "ui/pages.h"
#include "io/console.h"
#include "mem/alloc.h"
#include "plat/pscr.h"
#include "qbrt/qbrt.h"
#include "ui/screen.h"
#include "ui/theme.h"

static struct {
    uint32_t block; /* the kept screen's */
    bool kept;      /* the program's screen is in it, the screen in text mode */
} pg;

void upag_init(Session *const S) { pg.block = allo_new(&S->allo, qbrt_shot_size(), nullptr); }

void upag_in(Session *const S)
{
    if (qbrt_graphics() && !pg.kept) {
        qbrt_shot_take(at(allo_base(&S->allo, pg.block)));
        pg.kept = true;
        pscr_mode(VIDEO_TEXT);
    }

    pscr_page(PAGE_EDIT);
    them_in();
}

void upag_out(Session *const S)
{
    if (pg.kept) {
        qbrt_shot_show(at(allo_base(&S->allo, pg.block)));
        pg.kept = false;
        return;
    }

    pscr_page(PAGE_OUT);
    them_out();
}

void upag_clear(Session *const S)
{
    if (qbrt_graphics()) {
        qbrt_text();
        pg.kept = false;
        pscr_mode(VIDEO_TEXT);
    }

    them_out();
    pscr_page(PAGE_OUT);
    pscr_scroll(0, 0, 0, PSCR_ROWS - 1, PSCR_COLS - 1, ATTR_PLAIN);
    pscr_cursor(PAGE_OUT, 0, 0);
    S->cons.col = 0;
}

uint8_t upag_output(void) { return qbrt_graphics() ? PAGE_EDIT : PAGE_OUT; }
