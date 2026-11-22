/* The editor's and the output's pages: page 0 the editor's, page 1 the program's output.
 *
 * # How it works
 * A program in a graphics mode has the whole screen for its output. When the editor is shown, the
 * screen is kept (qbrt_shot_take) in a block taken for it and the screen put in text mode; the
 * runtime's state stays as the program left it, so a continue, a step or an immediate line draws
 * on. The output shown again sets the mode back and the screen with it (qbrt_shot_show), and the
 * block is given back. With no room to keep it, the program's screen goes back to text mode, as a
 * new run's. A run starts in text mode. The program's console is the runtime's, on the page shown (the BIOS's active one).
 *
 * # asm structure (retired/asm32/ui/ui.asm)
 * | asm | here | |
 * |---|---|---|
 * | ui_in, ui_out, ui_outclr | upag_in, upag_out, upag_clear | MIRROR |
 * | (none) | the kept screen | NEW |
 */
#include "ui/pages.h"
#include "io/console.h"
#include "plat/pmem.h"
#include "plat/pscr.h"
#include "qbrt/qbrt.h"
#include "ui/screen.h"
#include "ui/theme.h"

static struct {
    void *shot; /* the program's screen while the editor is shown, the screen in text mode */
} pg;

void upag_in(Session *const S)
{
    if (qbrt_graphics() && !pg.shot) {
        pg.shot = pmem_resize(nullptr, qbrt_shot_size());

        if (pg.shot)
            qbrt_shot_take(pg.shot);
        else
            qbrt_text();

        pscr_text(uimn.rows);
    } else if (pscr_rows() != uimn.rows) { /* the program's WIDTH, or its SCREEN 0 */
        pscr_text(uimn.rows);
    }

    pscr_page(PAGE_EDIT);
    them_in();
}

void upag_out(Session *const S)
{
    if (pg.shot) {
        qbrt_shot_show(pg.shot);
        pmem_free(pg.shot);
        pg.shot = nullptr;
        return;
    }

    pscr_page(PAGE_OUT);
    qbrt_pages_show(); /* the program's own visible page, where it has one */
    them_out();
}

void upag_clear(Session *const S)
{
    if (pg.shot) {
        qbrt_shot_drop();
        pmem_free(pg.shot);
        pg.shot = nullptr;
    }

    if (qbrt_graphics()) {
        qbrt_text();
        pscr_text(uimn.rows);
    }

    them_out();
    pscr_page(PAGE_OUT);
    qbrt_console_reset();
    S->cons.col = 0;
}
