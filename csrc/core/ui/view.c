/* Views: the module and each SUB or FUNCTION on its own (src/ui/view.asm). */
#include "ui/view.h"
#include "ui/dlog.h"
#include "ui/edit.h"
#include "ui/lmap.h"
#include "ui/text.h"
#include "ui/uimn.h"
#include "ui/wind.h"
#include "comp/comp.h"
#include "comp/decl.h"
#include "mem/copy.h"
#include "mem/prog.h"

/* The 0-ended s to d, its 0 too. */
static void copy_str(char *const d, const char *const s) { memcpy(d, s, strlen(s) + 1); }

enum { VLSZ = VNAME + 4 }; /* the SUBs box's list item */

static struct {
    uint8_t name_text[DLBUF]; /* first: name_field points here (ui.h's 16-bit note) */
    uint8_t n;                /* vw_n: the views */
    uint16_t new_len;         /* vw_nlen: add's line's length */
    View tab[NVIEW];
    uint8_t new_line[LNBUF];  /* vw_new: add's line */
    char list_text[NVIEW * VLSZ];
} vw;

static LineBuf name_field = {.text = vw.name_text}; /* vw_fld: New SUB's name */
static DlogList list;                                /* ls_v: the SUBs box's */

static const char u_nsub[] = "New SUB", u_nfn[] = "New FUNCTION", u_vname[] = "&Name:",
                  u_subs[] = "SUBs", u_vpick[] = "Choose program item to edit",
                  u_vact[] = "Edit in &Active", u_vspl[] = "Edit in &Split", u_vdel[] = "&Delete",
                  u_delp[] = "Delete the procedure?";

/* The New SUB, New FUNCTION box. */
static DlogCtl vnew_ctl[] = {
    {.kind = CT_FLD, .row = 1, .col = 10, .cols = 38, .rows = 1, .field = &name_field,
     .label = u_vname, .lrow = 2, .lcol = 2},
    {.kind = CT_BTN, .row = 6, .col = 8, .cols = 6, .text = uimn_ok},
    {.kind = CT_BTN, .row = 6, .col = 20, .cols = 10, .text = uimn_cancel},
    {.kind = CT_BTN, .row = 6, .col = 36, .cols = 8, .text = uimn_helpbtn, .hook = dlog_nohelp},
};
static Dlog dg_vnew = {.row = 8, .col = 14, .rows = 8, .cols = 52, .title = u_nsub, .n = 4,
                       .sep = 5, .def = 1, .ctl = vnew_ctl};
/* The SUBs box. */
static DlogCtl subs_ctl[] = {
    {.kind = CT_LST, .row = 2, .col = 2, .cols = 58, .rows = 12, .list = &list,
     .label = u_vpick, .lrow = 1, .lcol = 2},
    {.kind = CT_BTN, .row = 16, .col = 2, .cols = 17, .text = u_vact},
    {.kind = CT_BTN, .row = 16, .col = 20, .cols = 16, .text = u_vspl},
    {.kind = CT_BTN, .row = 16, .col = 37, .cols = 10, .text = u_vdel},
    {.kind = CT_BTN, .row = 16, .col = 48, .cols = 10, .text = uimn_cancel},
};
static Dlog dg_subs = {.row = 3, .col = 9, .rows = 18, .cols = 62, .title = u_subs, .n = 5,
                       .sep = 15, .def = 1, .ctl = subs_ctl};

/* The buttons dlog_run answers: New SUB's OK; the SUBs box's. */
enum : uint8_t { VNEW_OK = 1 };
enum : uint8_t { SUBS_ACTIVE = 1, SUBS_SPLIT = 2, SUBS_DELETE = 3 };

/* Text line line's body to comp_cbuf: decl_proc's answer; *name its M_VAR for PK_SUB, PK_FUNC. */
static uint8_t kind(const uint32_t line, const uint8_t **const name)
{
    prog_body(lmap_off(line), comp_cbuf);
    const uint8_t *nm = nullptr;
    const uint8_t k = decl_proc(comp_cbuf, &nm);
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
static void order(void)
{
    text_whole();
    prog_room(prog.end * 2 + 8); /* the lines copied after the store, then back */
    const uint32_t start = prog.end + 4;
    uint32_t to = start;
    for (uint8_t pass = 0; pass < 2; pass++) { /* 0: the module's lines, 1 the rest */
        bool in_proc = false;
        for (uint32_t line = 0; line < uimn.text.all; line++) {
            const uint8_t k = kind(line, nullptr);
            uint8_t mine = in_proc;
            if (k == PK_END)
                in_proc = false; /* the END is the procedure's */
            else if (k >= PK_SUB)
                in_proc = mine = true;
            if (mine != pass)
                continue;
            const uint32_t rec = lmap_off(line), len = prog_line(rec)->len;
            copy_prog(to, rec, len);
            to += len;
        }
    }
    copy_prog(PROGBASE, start, to - start);
    prog.end = PROGBASE + (to - start);
    prog_line(prog.end)->len = 0;
    lmap_build();
}

/* uimn.text.hook: a line added (delta 1) or removed (-1) in the active window's view: it and the
   views after it move. */
static void shift(const uint32_t line, const int32_t delta)
{
    const uint8_t v = uimn.ed.view;
    vw.tab[v].n += delta;
    for (uint8_t i = v + 1; i < vw.n; i++)
        vw.tab[i].first += delta;
}

/* The views from the text: the module, then a view from each SUB or FUNCTION line; the module
   never empty. */
static void scan(void)
{
    text_whole();
    uimn.text.hook = nullptr; /* not told of the module's line */
    if (is_proc(kind(0, nullptr)))
        text_insert(0, vw.new_line, 0); /* a blank line before */
    uimn.text.hook = shift;
    vw.tab[0].first = 0;
    vw.tab[0].kind = 0;
    vw.tab[0].name[0] = 0;
    uint32_t n = 1;
    for (uint32_t line = 1; line < uimn.text.all; line++) {
        const uint8_t *name;
        const uint8_t k = kind(line, &name);
        if (!is_proc(k) || n >= NVIEW)
            continue;
        View *const v = &vw.tab[n++];
        v->kind = k;
        v->first = line;
        const uint32_t len = name[1] < VNAME ? name[1] : VNAME; /* as typed: the M_VAR's */
        memcpy(v->name, name + 2, len);
        v->name[len] = 0;
    }
    vw.n = (uint8_t)n;
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

void view_reset(void)
{
    uimn.text.hook = shift;
    order();
    scan();
    for (uint32_t i = 0; i < NVIEW; i++) { /* no kept places */
        vw.tab[i].line = 0;
        vw.tab[i].top = 0;
        vw.tab[i].col = 0;
    }
    windows_on_module();
    view_apply();
}

void view_apply(void)
{
    if (uimn.ed.view >= vw.n)
        uimn.ed.view = 0;
    const View *const v = &vw.tab[uimn.ed.view];
    text_view(v->first, v->n);
}

/* View v in the active window, the cursor where it was left. */
static void show(const uint8_t v)
{
    edit_commit();
    View *const left = &vw.tab[uimn.ed.view]; /* the place left */
    left->line = uimn.ed.line;
    left->top = uimn.ed.top;
    left->col = uimn.edline.col;
    uimn.ed.view = v;
    view_apply();
    const View *const p = &vw.tab[v];
    uimn.ed.sel_on = false;
    uimn.ed.left = 0;
    uimn.focus = FOC_EDIT;
    uimn.ed.top = p->top;
    uimn.edline.col = p->col;
    uint32_t line = p->line;
    if (line >= uimn.text.n) {
        line = 0;
        uimn.ed.top = 0;
    }
    edit_load(line);
    edit_ensure();
}

uint8_t view_of(const uint32_t line)
{
    uint8_t v = 1;
    while (v < vw.n && line >= vw.tab[v].first)
        v++;
    return v - 1;
}

const View *view_at(const uint8_t v) { return &vw.tab[v]; }

uint32_t view_goline(const uint32_t line)
{
    const uint8_t v = view_of(line);
    if (v != uimn.ed.view)
        show(v);
    view_apply();
    return line - uimn.text.first;
}

void view_title(char *const title)
{
    if (!uimn.ed.view)
        return;
    const size_t n = strlen(title);
    title[n] = ':';
    copy_str(title + n + 1, vw.tab[uimn.ed.view].name);
}

void view_next(void)
{
    const uint8_t v = uimn.ed.view + 1;
    show(v < vw.n ? v : 0);
}

void view_prev(void)
{
    show(uimn.ed.view ? uimn.ed.view - 1 : vw.n - 1);
}

/* A procedure at the text's end: vw.new_line, a blank, its END (kind PK_SUB, PK_FUNC); shown,
   the cursor on the blank. */
static void add(const uint8_t k)
{
    text_whole();
    uimn.text.hook = nullptr;
    const uint32_t line = uimn.text.all;
    const char *const end = end_line(k);
    text_insert(line, vw.new_line, vw.new_len);
    text_insert(line + 1, vw.new_line, 0);
    text_insert(line + 2, (const uint8_t *)end, (uint16_t)strlen(end));
    touched();
    scan();
    View *const v = &vw.tab[vw.n - 1]; /* the last view, at its blank */
    v->line = 1;
    v->top = 0;
    v->col = 0;
    view_apply(); /* the old view's place kept */
    show(vw.n - 1);
}

void view_commit(void)
{
    const uint32_t line = uimn.ed.line;
    if (!line && uimn.ed.view) {
        scan(); /* its name may be another */
        view_apply();
        return;
    }
    const uint8_t k = kind(line + uimn.text.first, nullptr);
    if (!is_proc(k))
        return;
    vw.new_len = (uint16_t)text_get(uimn.ed.line, vw.new_line);
    text_delete(uimn.ed.line); /* out of its view */
    add(k);
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
static bool find(uint8_t *const v)
{
    for (uint8_t i = 1; i < vw.n; i++) {
        if (field_names(vw.tab[i].name)) {
            *v = i;
            return true;
        }
    }
    return false;
}

/* New SUB, New FUNCTION: kind k's name asked. */
static void new_proc(const uint8_t k, const char *const title)
{
    dg_vnew.title = title;
    dg_vnew.focus = 0;
    name_field.len = 0;
    name_field.col = 0;
    if (dlog_run(&dg_vnew) != VNEW_OK || !name_field.len)
        return;
    uint8_t v;
    if (find(&v)) {
        show(v);
        return;
    }
    const char *const kw = keyword(k); /* "SUB name" */
    const size_t n = strlen(kw);
    memcpy(vw.new_line, kw, n);
    vw.new_line[n] = ' ';
    memcpy(vw.new_line + n + 1, name_field.text, name_field.len);
    vw.new_len = (uint16_t)(n + 1 + name_field.len);
    add(k);
}

void view_newsub(void) { new_proc(PK_SUB, u_nsub); }
void view_newfn(void) { new_proc(PK_FUNC, u_nfn); }

/* View v's procedure out of the text, after asking; the module stays. */
static void del(const uint8_t v)
{
    if (!v || dlog_ync(u_delp) != YNC_YES)
        return;
    const uint32_t first = vw.tab[v].first, n = vw.tab[v].n; /* at least its SUB line */
    text_whole();
    uimn.text.hook = nullptr;
    for (uint32_t i = 0; i < n; i++)
        text_delete(first);
    touched();
    scan();
    windows_on_module(); /* the windows on it */
    view_apply();
    uimn.ed.top = 0;
    edit_load(0);
}

/* The SUBs box's list: the file's name, then each procedure's indented. */
static void list_fill(void)
{
    list.n = vw.n;
    for (uint8_t i = 0; i < vw.n; i++) {
        char *const d = vw.list_text + i * VLSZ;
        if (!i) {
            copy_str(d, uimn.file[0] ? uimn.file : uimn_untitled);
        } else {
            d[0] = d[1] = ' ';
            copy_str(d + 2, vw.tab[i].name);
        }
    }
    list.sel = uimn.ed.view;
    list.top = 0;
    list.items = vw.list_text;
    list.size = VLSZ;
}

void view_subs(void)
{
    edit_commit();
    list_fill();
    dg_subs.focus = 0;
    const uint8_t b = dlog_run(&dg_subs);
    const uint8_t v = (uint8_t)list.sel;
    if (b == SUBS_ACTIVE) {
        show(v);
    } else if (b == SUBS_SPLIT) { /* the split's other window shows it */
        if (!uimn.win.win[W_ED2].height)
            wind_split();
        wind_act(uimn.win.active == W_ED2 ? W_ED1 : W_ED2);
        show(v);
    } else if (b == SUBS_DELETE) {
        del(v);
    }
}
