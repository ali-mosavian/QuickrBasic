/* The debugger's call stack: the stop's frames, each with its line and the row it shows.
 *
 * # How it works
 * The frames are session_frames', NDFRM at most. A frame's row is its view's name, then "line n"
 * from column FRAME_LINE, n the line's in its view.
 */
#include "ui/debugstack.h"
#include "ui/debugvalue.h"
#include "ui/linemap.h"
#include "ui/view.h"

enum {
    NDFRM = 16,      /* the frames shown at most */
    FRAME_NAME = 15, /* a frame's row: its view's name, 15 characters at most, */
    FRAME_LINE = 16, /*   "line n" from column 16 */
};

static struct {
    uint8_t n;
    StackFrame fr[NDFRM];
} stk;

/* Frame f at at: its text line, view and row. */
static void frame1(Session *const S, StackFrame *const f, const DebugFrame *const at)
{
    f->at = *at;
    f->line = -1;
    f->view = DSTK_NOVIEW;
    memset(f->text, ' ', DP_TWMAX);
    f->text[DP_TWMAX] = 0;
    uint32_t tl;

    if (at->line == DLINE || !lmap_of(S, at->line, &tl)) {
        dval_cpy(f->text, "Immediate");
        return;
    }

    f->line = (int32_t)tl;
    const uint32_t v = view_of(tl);
    const View *const vw = view_at(v);
    const uint32_t n = tl - vw->first + 1;
    f->view = v;
    const char *const name = v ? view_name(v) : "module";

    for (int i = 0; i < FRAME_NAME && name[i]; i++)
        f->text[i] = name[i];

    dval_dec(dval_cpy(f->text + FRAME_LINE, "line "), (uint16_t)n);
}

void dstk_read(Session *const S)
{
    DebugFrame at[NDFRM];
    stk.n = (uint8_t)session_frames(S, at, NDFRM);

    for (uint8_t i = 0; i < stk.n; i++)
        frame1(S, &stk.fr[i], &at[i]);
}

uint8_t dstk_count(void) { return stk.n; }

const StackFrame *dstk_at(const uint8_t i) { return &stk.fr[i]; }

bool dstk_newest(const uint8_t i)
{
    for (uint8_t j = 0; j < i; j++)
        if (stk.fr[j].at.proc == stk.fr[i].at.proc)
            return false;

    return true;
}
