/* The Go To box: every procedure and file, narrowed as letters are typed, one shown.
 *
 * # How it works
 * The entries are the views: a procedure's by its name, its file beside it; a file's own lines
 * by the file's name, its folder aside. An entry is listed when the letters typed are in its name in order, case
 * aside ("drw" lists DrawWorld). Up and Down choose among those listed; Enter shows the view
 * (view_show), the place left kept for Alt+Left.
 */
#include "ui/goto.h"
#include "io/path.h"
#include "ui/dialog.h"
#include "ui/edit.h"
#include "ui/jumps.h"
#include "ui/mouse.h"
#include "ui/screen.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "util/grow.h"

enum : uint8_t {
    GT_ROW = 3,
    GT_COL = 14,
    GT_COLS = 52,
    GT_LIST = 12, /* the entries shown */
    GT_ROWS = GT_LIST + 4,
    GT_FILE = 30, /* a procedure's file's column in the box */
};

static struct {
    Widget w;
    LineBuf field;
    uint8_t text[DLBUF];
    uint32_t *shown; /* the views listed */
    uint32_t n, cap, sel, top;
} gt;

/* View v's file's name, its folder aside. */
static const char *file_of(Session *const S, const uint32_t v)
{
    const char *const path = view_file(S, v);
    return path + path_dir(path);
}

/* View v's name in the list: its procedure's, else its file's. */
static const char *label(Session *const S, const uint32_t v)
{
    return view_at(v)->kind ? view_name(v) : file_of(S, v);
}

/* The letters typed are in name in order, case aside. */
static bool fits(const char *const name)
{
    uint16_t k = 0;

    for (const char *p = name; *p && k < gt.field.len; p++)
        k += upcase((uint8_t)*p) == upcase(gt.field.text[k]);

    return k == gt.field.len;
}

/* The views that fit listed, the first chosen. */
static void narrow(Session *const S)
{
    gt.n = gt.sel = gt.top = 0;
    const uint32_t all = view_count();

    if (!GROW(gt.shown, gt.cap, all))
        return;

    for (uint32_t v = 0; v < all; v++)
        if (fits(label(S, v)))
            gt.shown[gt.n++] = v;
}

/* The entry chosen moved to sel, shown in the list. */
static void choose(const uint32_t sel)
{
    gt.sel = gt.n && sel >= gt.n ? gt.n - 1 : sel;

    if (gt.sel < gt.top)
        gt.top = gt.sel;
    if (gt.sel >= gt.top + GT_LIST)
        gt.top = gt.sel - GT_LIST + 1;
}

/* The box gone; with go the entry chosen shown. */
static void close(Session *const S, const bool go)
{
    gt.w.visible = false;

    if (!go || !gt.n)
        return;

    jumps_push();
    view_show(S, gt.shown[gt.sel]);
}

static bool gt_key(Widget *const w, Session *const S, const uint16_t key)
{
    const uint8_t scan = (uint8_t)(key >> 8), ch = (uint8_t)key;

    if (ch == CH_ESC)
        close(S, false);
    else if (ch == CH_ENTER)
        close(S, true);
    else if (!ch && scan == SCAN_UP)
        choose(gt.sel ? gt.sel - 1 : 0);
    else if (!ch && scan == SCAN_DOWN)
        choose(gt.sel + 1);
    else if (!ch && scan == SCAN_PGUP)
        choose(gt.sel > GT_LIST ? gt.sel - GT_LIST : 0);
    else if (!ch && scan == SCAN_PGDN)
        choose(gt.sel + GT_LIST);
    else {
        const uint16_t len = gt.field.len;
        const bool typed = dlog_ledit(&gt.field, key);

        if (typed && (gt.field.len != len || ch >= ' '))
            narrow(S);
    }

    return true;
}

static bool gt_mouse(Widget *const w, Session *const S, const uint8_t row, const uint8_t col)
{
    if (uimn.mouse.event != MS_PRESS)
        return true;

    const bool in =
        row >= GT_ROW && row < GT_ROW + GT_ROWS && col >= GT_COL && col < GT_COL + GT_COLS;
    const uint32_t at = gt.top + (uint32_t)(row - GT_ROW - 3);

    if (!in)
        close(S, false);
    else if (row >= GT_ROW + 3 && at < gt.n) {
        choose(at);
        close(S, true);
    }

    return true;
}

static void gt_draw(Widget *const w, Session *const S)
{
    scrn_box(GT_ROW, GT_COL, GT_ROWS, GT_COLS, SL_DLG, "Go To");
    scrn_shadow(GT_ROW, GT_COL, GT_ROWS, GT_COLS);
    const uint16_t width = GT_COLS - 4;
    const uint16_t left = gt.field.col >= width ? (uint16_t)(gt.field.col - width + 1) : 0;
    const uint16_t shown = gt.field.len > left ? gt.field.len - left : 0;
    scrn_strn(GT_ROW + 1, GT_COL + 2, gt.field.text + left, shown < width ? shown : width, SL_DLG);
    scrn_sep(GT_ROW + 2, GT_COL, GT_COLS, SL_DLG);

    for (uint32_t i = 0; i < GT_LIST && gt.top + i < gt.n; i++) {
        const uint32_t v = gt.shown[gt.top + i];
        const uint8_t row = (uint8_t)(GT_ROW + 3 + i),
                      slot = gt.top + i == gt.sel ? SL_DSEL : SL_DLG;
        const char *const name = label(S, v);
        const uint32_t len = strlen(name), most = view_at(v)->kind ? GT_FILE - 3 : width;
        scrn_fill(row, GT_COL + 1, slot, ' ', GT_COLS - 2);
        scrn_strn(
            row,
            GT_COL + 2,
            (const uint8_t *)name,
            (uint16_t)(len < most ? len : most),
            slot
        );

        if (view_at(v)->kind) {
            const char *const file = file_of(S, v);
            const uint32_t flen = strlen(file), room = GT_COLS - GT_FILE - 2;
            scrn_strn(
                row,
                GT_COL + GT_FILE,
                (const uint8_t *)file,
                (uint16_t)(flen < room ? flen : room),
                slot
            );
        }
    }
}

static void gt_cursor(Widget *const w)
{
    const uint16_t width = GT_COLS - 4;
    const uint16_t left = gt.field.col >= width ? (uint16_t)(gt.field.col - width + 1) : 0;
    scrn_shape(CURSOR_INSERT);
    scrn_cursor(GT_ROW + 1, (uint8_t)(GT_COL + 2 + gt.field.col - left));
}

static const WidgetOps gt_ops = {
    .draw = gt_draw,
    .key = gt_key,
    .mouse = gt_mouse,
    .cursor = gt_cursor,
};

void goto_build(Widget *const screen)
{
    gt.w = (Widget){.ops = &gt_ops, .rect = {GT_ROW, GT_COL, GT_ROWS, GT_COLS}, .modal = true};

    gt.field = (LineBuf){.text = gt.text, .cap = DLBUF};

    tree_add(screen, &gt.w);
}

void goto_run(Session *const S)
{
    edit_commit(S);
    gt.field.len = gt.field.col = 0;
    narrow(S);
    uimn_modal(S, &gt.w);
}
