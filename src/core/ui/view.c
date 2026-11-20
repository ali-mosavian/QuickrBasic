/* Views: the module and each SUB or FUNCTION on its own.
 *
 * # How it works
 * The text keeps the module's lines first, then each procedure from its SUB or FUNCTION line to the
 * next one's (view_reset orders it so). A view is a part: first, n lines. A window's uimn.ed.view
 * is its view, applied to the text (text_view) by view_apply.
 *
 * A line added or removed (uimn.text.hook) moves the views after the active window's. The cursor's
 * place in a view is kept when another is shown.
 *
 * # asm structure (retired/asm32/ui/view.asm)
 * | asm | here | |
 * |---|---|---|
 * | vw_reset, vw_apply | view_reset, view_apply | MIRROR |
 * | vw_goline, vw_of | view_goline, view_of; view_at | MIRROR; D1 |
 * | vw_title | view_name; draw.c builds the title | D2 |
 * | vw_next, vw_prev, vw_commit, vw_newsub, vw_newfn, vw_subs | view_next, view_prev, view_commit, view_newsub, view_newfn, view_subs | MIRROR |
 * | vw_kind, vw_order, vw_scan, vw_shift, vw_ptr, vw_show, vw_cpy, vw_find, vw_add, vw_del | statics | MIRROR |
 * | TVIEW | View (first, n, kind, line, top, col, name) | MIRROR |
 * | g_ui.vw.*, bf.vw_new, vw_fld, vw_list, dg.ls_v | statics | MIRROR |
 * | NVIEW, VNAME, VLSZ | none: the views, names and list grow | D3 |
 * | u_kwsub .. u_delp, dg_vnew, dg_subs | statics | MIRROR |
 *
 * - D1. vw_of's second result, the table entry, is view_at(v).
 * - D2. The title is any length: draw.c grows its buffer.
 * - D3. No limit on the views or their names' length; the list's items as wide as its longest.
 */
#include "ui/view.h"
#include "session.h"
#include "ui/dialog.h"
#include "ui/edit.h"
#include "ui/indent.h"
#include "ui/linemap.h"
#include "ui/text.h"
#include "ui/uimain.h"
#include "ui/window.h"
#include "comp/comp.h"
#include "comp/declare.h"
#include "mem/copy.h"
#include "mem/program.h"
#include "util/grow.h"

/* The 0-ended s to d, its 0 too. */
static void copy_str(char *const d, const char *const s) { memcpy(d, s, strlen(s) + 1); }

static struct {
    uint8_t name_text[DLBUF]; /* first: name_field points here (ui.h's 16-bit note) */
    uint32_t n;               /* vw_n: the views */
    uint32_t cap;
    View *tab;
    char *names; /* the views' names, each 0-ended: View.name's offsets */
    uint32_t nlen, ncap;
    LineBuf new_line; /* vw_new, vw_nlen: add's line, grown to fit */
    char *list_text;
    uint32_t list_cap;
    uint32_t *order; /* the SUBs list's views: the module, then the procedures by name */
    uint32_t order_cap;
} vw = {.new_line = {.grows = true}};

static LineBuf name_field = {.text = vw.name_text, .cap = DLBUF}; /* vw_fld: New SUB's name */
static DlogList list = {.columns = true};                         /* ls_v: the SUBs box's */

static const char u_nsub[] = "New SUB", u_nfn[] = "New FUNCTION",
                  u_vname[] = "&Name:", u_subs[] = "SUBs",
                  u_vpick[] = "Choose program item to edit", u_vact[] = "Edit in &Active",
                  u_vspl[] = "Edit in &Split", u_vdel[] = "&Delete",
                  u_delp[] = "Delete the procedure?";

/* The New SUB, New FUNCTION box. */
static DlogCtl vnew_ctl[] = {
    {.kind = CT_FLD,
     .row = 1,
     .col = 10,
     .cols = 38,
     .rows = 1,
     .field = &name_field,
     .label = u_vname,
     .lrow = 2,
     .lcol = 2},
    {.kind = CT_BTN, .row = 6, .col = 8, .cols = 6, .text = uimn_ok},
    {.kind = CT_BTN, .row = 6, .col = 20, .cols = 10, .text = uimn_cancel},
    {.kind = CT_BTN, .row = 6, .col = 36, .cols = 8, .text = uimn_helpbtn, .hook = dlog_nohelp},
};
static Dlog dg_vnew = {
    .row = 8,
    .col = 14,
    .rows = 8,
    .cols = 52,
    .title = u_nsub,
    .n = 4,
    .sep = 5,
    .def = 1,
    .ctl = vnew_ctl,
};
/* The SUBs box: QBasic's width (uiview.des). */
static DlogCtl subs_ctl[] = {
    {.kind = CT_LST,
     .row = 2,
     .col = 2,
     .cols = 70,
     .rows = 12,
     .list = &list,
     .label = u_vpick,
     .lrow = 1,
     .lcol = 2},
    {.kind = CT_BTN, .row = 17, .col = 5, .cols = 17, .text = u_vact},
    {.kind = CT_BTN, .row = 17, .col = 26, .cols = 16, .text = u_vspl},
    {.kind = CT_BTN, .row = 17, .col = 46, .cols = 10, .text = u_vdel},
    {.kind = CT_BTN, .row = 17, .col = 60, .cols = 10, .text = uimn_cancel},
};
static Dlog dg_subs = {
    .row = 2,
    .col = 2,
    .rows = 19,
    .cols = 76,
    .title = u_subs,
    .n = 5,
    .sep = 16,
    .def = 1,
    .ctl = subs_ctl,
};

/* The buttons dlog_run answers: New SUB's OK; the SUBs box's. */
enum : uint8_t { VNEW_OK = 1 };
enum : uint8_t { SUBS_ACTIVE = 1, SUBS_SPLIT = 2, SUBS_DELETE = 3 };

/* Text line line's body to comp_cbuf: decl_proc's answer; *name its M_VAR for PK_SUB, PK_FUNC. */
static uint8_t kind(Session *const S, const uint32_t line, const uint8_t **const name)
{
    prog_body(&S->prog, lmap_off(S, line), S->comp.cbuf);
    const uint8_t *nm = nullptr;
    const uint8_t k = decl_proc(S->comp.cbuf, &nm);

    if (name)
        *name = nm;

    return k;
}

static bool is_proc(const uint8_t k) { return k >= PK_SUB && k < PK_END; }

/* Kind k's keyword, its END line. */
static const char *keyword(const uint8_t k) { return k == PK_SUB ? "SUB" : "FUNCTION"; }
static const char *end_line(const uint8_t k) { return k == PK_SUB ? "END SUB" : "END FUNCTION"; }

/* The text's module lines first, then the procedures', each from its SUB or FUNCTION line to its
   END; in their order. */
static void order(Session *const S)
{
    text_whole();
    prog_room(&S->prog, S->prog.end * 2 + 8); /* the lines copied after the store, then back */
    const uint32_t start = S->prog.end + 4;
    uint32_t to = start;

    for (uint8_t pass = 0; pass < 2; pass++) { /* 0: the module's lines, 1 the rest */
        bool in_proc = false;

        for (uint32_t line = 0; line < uimn.text.all; line++) {
            const uint8_t k = kind(S, line, nullptr);
            uint8_t mine = in_proc;

            if (k == PK_END)
                in_proc = false; /* the END is the procedure's */
            else if (k >= PK_SUB)
                in_proc = mine = true;
            if (mine != pass)
                continue;

            const uint32_t rec = lmap_off(S, line), len = prog_line(&S->prog, rec)->len;
            prog_copy(&S->prog, to, rec, len);
            to += len;
        }
    }

    prog_copy(&S->prog, PROGBASE, start, to - start);
    S->prog.end = PROGBASE + (to - start);
    prog_line(&S->prog, S->prog.end)->len = 0;
    lmap_build(S);
}

/* uimn.text.hook: a line added (delta 1) or removed (-1) in the active window's view: it and the
   views after it move. */
static void shift(const uint32_t line, const int32_t delta)
{
    const uint32_t v = uimn.ed.view;
    vw.tab[v].n += delta;

    for (uint32_t i = v + 1; i < vw.n; i++)
        vw.tab[i].first += delta;
}

/* The views from the text: the module, then a view from each SUB or FUNCTION line; the module
   never empty. */
static void scan(Session *const S)
{
    text_whole();
    uimn.text.hook = nullptr; /* not told of the module's line */
    if (is_proc(kind(S, 0, nullptr)))
        text_insert(S, 0, (const uint8_t *)"", 0); /* a blank line before */
    uimn.text.hook = shift;
    vw.n = 0;

    /* no memory: no views, the whole text shown; for a procedure, its lines the last view's */
    if (!GROW(vw.tab, vw.cap, 1) || !GROW(vw.names, vw.ncap, 1))
        return;

    vw.tab[0].first = 0;
    vw.tab[0].kind = 0;
    vw.tab[0].name = 0;
    vw.names[0] = 0;
    vw.nlen = 1;
    uint32_t n = 1;

    for (uint32_t line = 1; line < uimn.text.all; line++) {
        const uint8_t *name;
        const uint8_t k = kind(S, line, &name);
        const uint32_t len = name ? name[1] : 0; /* as typed: the M_VAR's */

        if (!is_proc(k) || !GROW(vw.tab, vw.cap, n + 1) ||
            !GROW(vw.names, vw.ncap, vw.nlen + len + 1))
            continue;

        View *const v = &vw.tab[n++];
        v->kind = k;
        v->first = line;
        v->name = vw.nlen;
        memcpy(vw.names + vw.nlen, name + 2, len);
        vw.nlen += len;
        vw.names[vw.nlen++] = 0;
    }

    vw.n = n;

    for (uint32_t i = 0; i < n; i++) /* each one's lines: to the next */
        vw.tab[i].n = (i + 1 < n ? vw.tab[i + 1].first : uimn.text.all) - vw.tab[i].first;
}

/* Both edit windows on the module. */
static void windows_on_module(void)
{
    uimn.ed.view = 0;
    uimn.win.win[W_ED1].ed.view = 0;
    uimn.win.win[W_ED2].ed.view = 0;
}

/* The text changed by the views: since entered, since saved. */
static void touched(void)
{
    uimn.text.changed = true;
    uimn.text.unsaved = true;
}

void view_reset(Session *const S)
{
    uimn.text.hook = shift;
    order(S);
    scan(S);

    for (uint32_t i = 0; i < vw.cap; i++) { /* no kept places */
        vw.tab[i].line = 0;
        vw.tab[i].top = 0;
        vw.tab[i].col = 0;
    }

    windows_on_module();
    view_apply();
}

void view_apply(void)
{
    if (!vw.n) {
        text_whole();
        return;
    }

    if (uimn.ed.view >= vw.n)
        uimn.ed.view = 0;

    const View *const v = &vw.tab[uimn.ed.view];
    text_view(v->first, v->n);
}

/* View v in the active window, the cursor where it was left. */
static void show(Session *const S, const uint32_t v)
{
    edit_commit(S);
    View *const left = &vw.tab[uimn.ed.view]; /* the place left */
    left->line = uimn.ed.line;
    left->top = uimn.ed.top;
    left->col = uimn.edline.col;
    uimn.ed.view = v;
    view_apply();
    const View *const p = &vw.tab[v];
    uimn.ed.sel_on = false;
    uimn.ed.left = 0;
    uimn.tree.workspace.focus = FOC_EDIT;
    uimn.ed.top = p->top;
    uimn.edline.col = p->col;
    uint32_t line = p->line;

    if (line >= uimn.text.n) {
        line = 0;
        uimn.ed.top = 0;
    }

    edit_load(S, line);
    edit_ensure();
}

uint32_t view_of(const uint32_t line)
{
    uint32_t v = 1;

    while (v < vw.n && line >= vw.tab[v].first)
        v++;

    return v - 1;
}

const View *view_at(const uint32_t v) { return &vw.tab[v]; }

const char *view_name(const uint32_t v) { return vw.names + vw.tab[v].name; }

uint32_t view_proc(Session *const S, const uint32_t v)
{
    const uint8_t *nm = nullptr;
    return vw.tab[v].kind && is_proc(kind(S, vw.tab[v].first, &nm)) ? comp_nameof(nm) : 0;
}

uint32_t view_goline(Session *const S, const uint32_t line)
{
    const uint32_t v = view_of(line);

    if (v != uimn.ed.view)
        show(S, v);

    view_apply();
    return line - uimn.text.first;
}

void view_next(Session *const S)
{
    const uint32_t v = uimn.ed.view + 1;
    show(S, v < vw.n ? v : 0);
}

void view_prev(Session *const S) { show(S, uimn.ed.view ? uimn.ed.view - 1 : vw.n - 1); }

/* A procedure at the text's end: vw.new_line, a blank a step in, its END (kind PK_SUB,
   PK_FUNC); shown, the cursor on the blank. */
static void add(Session *const S, const uint8_t k)
{
    text_whole();
    uimn.text.hook = nullptr;
    const uint32_t line = uimn.text.all;
    const char *const end = end_line(k);
    uint8_t body[INDT_STEP];
    memset(body, ' ', sizeof body);
    text_insert(S, line, vw.new_line.text, vw.new_line.len);
    text_insert(S, line + 1, body, sizeof body);
    text_insert(S, line + 2, (const uint8_t *)end, (uint16_t)strlen(end));
    touched();
    scan(S);
    View *const v = &vw.tab[vw.n - 1]; /* the last view, at its blank */
    v->line = 1;
    v->top = 0;
    v->col = INDT_STEP;
    view_apply(); /* the old view's place kept */
    show(S, vw.n - 1);
}

void view_commit(Session *const S)
{
    const uint32_t line = uimn.ed.line;

    if (!line && uimn.ed.view) {
        scan(S); /* its name may be another */
        view_apply();
        return;
    }

    const uint8_t k = kind(S, line + uimn.text.first, nullptr);

    if (!is_proc(k))
        return;

    vw.new_line.len = (uint16_t)text_get(S, uimn.ed.line, &vw.new_line);
    text_delete(S, uimn.ed.line); /* out of its view */
    add(S, k);
}

/* Name s the field's, case aside. */
static bool field_names(const char *const s)
{
    uint16_t j = 0;

    while (j < name_field.len && upcase((uint8_t)s[j]) == upcase(name_field.text[j]))
        j++;

    return j == name_field.len && !s[j];
}

/* The view named as the field; false: none. */
static bool find(uint32_t *const v)
{
    for (uint32_t i = 1; i < vw.n; i++) {
        if (field_names(view_name(i))) {
            *v = i;
            return true;
        }
    }

    return false;
}

/* New SUB, New FUNCTION: kind k's name asked. */
static void new_proc(Session *const S, const uint8_t k, const char *const title)
{
    dg_vnew.title = title;
    dg_vnew.focus = 0;
    name_field.len = 0;
    name_field.col = 0;

    if (dlog_run(S, &dg_vnew) != VNEW_OK || !name_field.len)
        return;

    uint32_t v;

    if (find(&v)) {
        show(S, v);
        return;
    }

    const char *const kw = keyword(k); /* "SUB name" */
    const size_t n = strlen(kw);
    vw.new_line.len = (uint16_t)(n + 1 + name_field.len);
    uint8_t *const t = edit_lb_need(S, &vw.new_line, vw.new_line.len);
    memcpy(t, kw, n);
    t[n] = ' ';
    memcpy(t + n + 1, name_field.text, name_field.len);
    add(S, k);
}

void view_newsub(Session *const S) { new_proc(S, PK_SUB, u_nsub); }
void view_newfn(Session *const S) { new_proc(S, PK_FUNC, u_nfn); }

/* View v's procedure out of the text, after asking; the module stays. */
static void del(Session *const S, const uint32_t v)
{
    if (!v || dlog_ync(S, u_delp) != YNC_YES)
        return;

    const uint32_t first = vw.tab[v].first, n = vw.tab[v].n; /* at least its SUB line */
    text_whole();
    uimn.text.hook = nullptr;

    for (uint32_t i = 0; i < n; i++)
        text_delete(S, first);

    touched();
    scan(S);
    windows_on_module(); /* the windows on it */
    view_apply();
    uimn.ed.top = 0;
    edit_load(S, 0);
}

/* The SUBs box's list: the file's name, then each procedure's indented, by name; each item as
   wide as the longest. false: no memory. */
static bool list_fill(void)
{
    const char *const file = uimn.file[0] ? uimn.file : uimn_untitled;
    uint32_t size = (uint32_t)strlen(file) + 1;

    for (uint32_t i = 1; i < vw.n; i++) {
        const uint32_t len = (uint32_t)strlen(view_name(i)) + 3; /* two spaces in, its 0 */
        size = len > size ? len : size;
    }

    if (!grow_room(&vw.list_text, &vw.list_cap, vw.n * size, 1) ||
        !GROW(vw.order, vw.order_cap, vw.n))
        return false;

    list.sel = 0;

    for (uint32_t i = 0; i < vw.n; i++) { /* insertion: the module, 0, stays first */
        uint32_t at = i;

        for (; at > 1 && dlog_namecmp(view_name(vw.order[at - 1]), view_name(i)) > 0; at--)
            vw.order[at] = vw.order[at - 1];

        vw.order[at] = i;
    }

    for (uint32_t k = 0; k < vw.n; k++) {
        const uint32_t v = vw.order[k];
        char *const d = vw.list_text + k * size;

        if (v == uimn.ed.view)
            list.sel = k;

        if (!v) {
            copy_str(d, file);
        } else {
            d[0] = d[1] = ' ';
            copy_str(d + 2, view_name(v));
        }
    }

    list.n = vw.n;
    list.top = 0;
    list.items = vw.list_text;
    list.size = size;
    return true;
}

void view_subs(Session *const S)
{
    edit_commit(S);

    if (!list_fill())
        return;

    dg_subs.focus = 0;
    const uint8_t b = dlog_run(S, &dg_subs);
    const uint32_t v = list.n ? vw.order[list.sel] : 0;

    if (b == SUBS_ACTIVE) {
        show(S, v);
    } else if (b == SUBS_SPLIT) { /* the split's other window shows it */
        if (!uimn.win.win[W_ED2].height)
            wind_split(S);

        wind_act(S, wind_active() == W_ED2 ? W_ED1 : W_ED2);
        show(S, v);
    } else if (b == SUBS_DELETE) {
        del(S, v);
    }
}
