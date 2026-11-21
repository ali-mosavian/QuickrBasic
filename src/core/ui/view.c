/* Views: each file's module and each SUB or FUNCTION on its own.
 *
 * # How it works
 * The text is each of the program's files, the main's first (text.h). A file keeps its module's
 * lines first, then each procedure from its SUB or FUNCTION line to the next one's (order() makes
 * it so). A view is a part: first, n lines, of one file. A window's uimn.ed.view is its view,
 * applied to the text (text_view) by view_apply.
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
#include "comp/import.h"
#include "io/repl.h"
#include "mem/copy.h"
#include "mem/module.h"
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
    uint32_t *order; /* the SUBs list's views: each file's module, then its procedures by name */
    uint32_t order_cap;
    uint32_t epoch, drops; /* the modules' (Modu's) when the views were made */
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

/* Record rec's body to comp_cbuf: decl_proc's answer; *name its M_VAR for PK_SUB, PK_FUNC. */
static uint8_t kind_of(Session *const S, const uint32_t rec, const uint8_t **const name)
{
    prog_body(&S->prog, rec, S->comp.cbuf);
    const uint8_t *nm = nullptr;
    const uint8_t k = decl_proc(S->comp.cbuf, &nm);

    if (name)
        *name = nm;

    return k;
}

/* The same, of text line line. */
static uint8_t kind(Session *const S, const uint32_t line, const uint8_t **const name)
{
    return kind_of(S, lmap_off(S, line), name);
}

static bool is_proc(const uint8_t k) { return k >= PK_SUB && k < PK_END; }

/* View v a procedure's. */
static bool proc_view(const uint32_t v) { return v < vw.n && vw.tab[v].kind; }

/* Kind k's keyword, its END line. */
static const char *keyword(const uint8_t k) { return k == PK_SUB ? "SUB" : "FUNCTION"; }
static const char *end_line(const uint8_t k) { return k == PK_SUB ? "END SUB" : "END FUNCTION"; }

/* Record rec and the include's after it (LF_INC): their bytes. */
static uint32_t with_includes(const Prog *const pg, const uint32_t rec)
{
    uint32_t end = rec + prog_line(pg, rec)->len;

    while (prog_line(pg, end)->len && prog_line(pg, end)->flags & LF_INC)
        end += prog_line(pg, end)->len;

    return end - rec;
}

/* Each file's records, from to its LF_TOP or the end: its module's lines first, then the
   procedures', each from its SUB or FUNCTION line to its END; in their order. An include's records
   go with its line; the LF_TOP lines stay between the files. */
static void order(Session *const S)
{
    Prog *const pg = &S->prog;
    text_whole();
    prog_room(pg, pg->end * 2 + 8); /* the lines copied after the store, then back */
    const uint32_t start = pg->end + 4;
    uint32_t to = start;

    for (uint32_t from = PROGBASE; prog_line(pg, from)->len;) {
        uint32_t end = from; /* the file's LF_TOP, else the terminator */

        while (prog_line(pg, end)->len && !(prog_line(pg, end)->flags & LF_TOP))
            end += prog_line(pg, end)->len;

        for (uint8_t pass = 0; pass < 2; pass++) { /* 0: the module's lines, 1 the rest */
            bool in_proc = false;

            for (uint32_t rec = from; rec < end;) {
                const uint8_t k = kind_of(S, rec, nullptr);
                const uint32_t len = with_includes(pg, rec);
                uint8_t mine = in_proc;

                if (k == PK_END)
                    in_proc = false; /* the END is the procedure's */
                else if (k >= PK_SUB)
                    in_proc = mine = true;
                if (mine == pass) {
                    prog_copy(pg, to, rec, len);
                    to += len;
                }

                rec += len;
            }
        }

        const uint32_t top = prog_line(pg, end)->len;
        prog_copy(pg, to, end, top);
        to += top;
        from = end + top;
    }

    prog_copy(pg, PROGBASE, start, to - start);
    pg->end = PROGBASE + (to - start);
    prog_line(pg, pg->end)->len = 0;
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

/* Text line line starts a file. */
static bool starts_file(Session *const S, const uint32_t line)
{
    return !line || text_module(S, line) != text_module(S, line - 1);
}

/* The views from the text: each file's module, then a view from each SUB or FUNCTION line; a
   file's module never empty. */
static void scan(Session *const S)
{
    text_whole();
    uimn.text.hook = nullptr; /* not told of the files' first lines */

    for (uint32_t line = 0; line < uimn.text.all; line++) {
        if (!starts_file(S, line) || !is_proc(kind(S, line, nullptr)))
            continue;

        const uint32_t m = text_module(S, line);
        const bool edited = text_unsaved(S, m);
        text_insert(S, line++, (const uint8_t *)"", 0); /* a blank line before, the file's */

        if (!edited) /* a view's, not an edit */
            text_saved(S, m);
    }

    uimn.text.hook = shift;
    vw.n = 0;
    vw.epoch = S->modu.epoch;
    vw.drops = S->modu.drops;

    /* no memory: no views, the whole text shown; for a procedure, its lines the last view's */
    if (!GROW(vw.names, vw.ncap, 1))
        return;

    vw.names[0] = 0;
    vw.nlen = 1;
    uint32_t n = 0;

    for (uint32_t line = 0; line < uimn.text.all; line++) {
        const uint8_t *name;
        const uint8_t k = kind(S, line, &name);
        const bool file = !n || starts_file(S, line);
        const uint32_t len = !file && name ? name[1] : 0; /* as typed: the M_VAR's */

        if ((!file && !is_proc(k)) || !GROW(vw.tab, vw.cap, n + 1) ||
            !GROW(vw.names, vw.ncap, vw.nlen + len + 1))
            continue;

        View *const v = &vw.tab[n++];
        v->kind = file ? 0 : k;
        v->module = text_module(S, line);
        v->first = line;
        v->name = file ? 0 : vw.nlen;

        if (!file) {
            memcpy(vw.names + vw.nlen, name + 2, len);
            vw.nlen += len;
            vw.names[vw.nlen++] = 0;
        }
    }

    vw.n = n;

    for (uint32_t i = 0; i < n; i++) /* each one's lines: to the next */
        vw.tab[i].n = (i + 1 < n ? vw.tab[i + 1].first : uimn.text.all) - vw.tab[i].first;
}

/* Both edit windows on the main's module. */
static void windows_on_module(void)
{
    uimn.ed.view = 0;
    uimn.win.win[W_ED1].ed.view = 0;
    uimn.win.win[W_ED2].ed.view = 0;
}

/* View v the place kept in view from. */
static void keep_place(View *const v, const View *const from)
{
    v->line = from->line;
    v->top = from->top;
    v->col = from->col;
}

/* After scan: a view added at at (delta 1), or the one at at gone (-1). The views past it move:
   each window's, the places kept in them; a window on the one gone to view to. */
static void views_moved(const uint32_t at, const int32_t delta, const uint32_t to)
{
    uint32_t *const views[] =
        {&uimn.ed.view, &uimn.win.win[W_ED1].ed.view, &uimn.win.win[W_ED2].ed.view};

    for (uint32_t i = 0; i < sizeof views / sizeof views[0]; i++) {
        uint32_t *const v = views[i];

        if (*v == at && delta < 0)
            *v = to;
        else if (*v >= at)
            *v = (uint32_t)((int32_t)*v + delta);
    }

    if (delta > 0) /* scan left them where they were */
        for (uint32_t i = vw.n - 1; i > at; i--)
            keep_place(&vw.tab[i], &vw.tab[i - 1]);
    else
        for (uint32_t i = at; i < vw.n; i++)
            keep_place(&vw.tab[i], &vw.tab[i + 1]);
}

/* View v's file's first view, its module's. */
static uint32_t file_view(const uint32_t v)
{
    uint32_t f = v;

    while (f && vw.tab[f].kind)
        f--;

    return f;
}

/* The text changed by the views: since entered. */
static void touched(void) { uimn.text.changed = true; }

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

uint32_t view_module(const uint32_t v) { return v < vw.n ? vw.tab[v].module : 0; }

const char *view_file(Session *const S, const uint32_t v)
{
    const uint32_t m = view_module(v);

    if (m)
        return modu_module_path(&S->modu, m);

    return uimn.file[0] ? uimn.file : uimn_untitled;
}

bool view_modules(Session *const S)
{
    lmap_build(S);

    if (S->modu.epoch == vw.epoch)
        return false;

    const bool dropped = S->modu.drops != vw.drops;
    order(S);
    scan(S);

    if (dropped) /* the views past it others now */
        windows_on_module();

    view_apply();
    return true;
}

uint32_t view_proc(Session *const S, const uint32_t v)
{
    const uint8_t *nm = nullptr;
    return vw.tab[v].kind && is_proc(kind(S, vw.tab[v].first, &nm)) ? comp_nameof(nm) : 0;
}

uint32_t view_procedure(Session *const S, const uint32_t module, const uint32_t name)
{
    for (uint32_t v = 1; v < vw.n; v++)
        if (proc_view(v) && vw.tab[v].module == module && view_proc(S, v) == name)
            return v;

    return 0;
}

uint32_t view_of_module(const uint32_t module)
{
    for (uint32_t v = 0; v < vw.n; v++)
        if (!vw.tab[v].kind && vw.tab[v].module == module)
            return v;

    return 0;
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

/* A procedure at the end of the active view's file: vw.new_line, a blank a step in, its END (kind
   PK_SUB, PK_FUNC); shown, the cursor on the blank. */
static void add(Session *const S, const uint8_t k)
{
    const uint32_t file = file_view(uimn.ed.view);
    uint32_t last = file; /* the file's last view */

    while (last + 1 < vw.n && vw.tab[last + 1].kind)
        last++;

    const uint32_t first = vw.tab[file].first, end = vw.tab[last].first + vw.tab[last].n;
    text_view(first, end - first); /* the lines added the file's */
    uimn.text.hook = nullptr;
    const char *const end_text = end_line(k);
    uint8_t body[INDT_STEP];
    memset(body, ' ', sizeof body);
    const uint32_t at = end - first;
    text_insert(S, at, vw.new_line.text, vw.new_line.len);
    text_insert(S, at + 1, body, sizeof body);
    text_insert(S, at + 2, (const uint8_t *)end_text, (uint16_t)strlen(end_text));
    touched();
    scan(S);
    views_moved(last + 1, 1, 0);
    View *const v = &vw.tab[last + 1]; /* the new view, at its blank */
    v->line = 1;
    v->top = 0;
    v->col = INDT_STEP;
    view_apply(); /* the old view's place kept */
    show(S, last + 1);
}

/* Text line line (the active view's) an IMPORT or FROM: the modules it names loaded, shown. */
static void imports(Session *const S, const uint32_t line)
{
    prog_body(&S->prog, lmap_off(S, line), S->comp.cbuf);

    if (!impt_line(S->comp.cbuf, nullptr, nullptr))
        return;

    const Status st = repl_call(S, repl_open);
    view_modules(S);
    edit_recheck(S);
    fault_check(&S->fault, st);
}

void view_commit(Session *const S)
{
    const uint32_t line = uimn.ed.line;

    if (!line && proc_view(uimn.ed.view)) {
        scan(S); /* its name may be another */
        view_apply();
        return;
    }

    const uint8_t k = kind(S, line + uimn.text.first, nullptr);

    if (!is_proc(k)) {
        imports(S, line + uimn.text.first);
        return;
    }

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

/* The active view's file's procedure named as the field; false: none. */
static bool find(uint32_t *const v)
{
    for (uint32_t i = 1; i < vw.n; i++) {
        if (proc_view(i) && vw.tab[i].module == view_module(uimn.ed.view) &&
            field_names(view_name(i))) {
            *v = i;
            return true;
        }
    }

    return false;
}

/* New SUB, New FUNCTION: kind k's name asked. */
static void new_proc(Session *const S, const uint8_t k, const char *const title)
{
    if (edit_refused(S)) /* it goes in the active view's file */
        return;

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

/* View v's procedure out of the text, after asking; a file's module stays. */
static void del(Session *const S, const uint32_t v)
{
    if (!proc_view(v) || edit_refused_in(S, v) || dlog_ync(S, u_delp) != YNC_YES)
        return;

    const uint32_t first = vw.tab[v].first, n = vw.tab[v].n; /* at least its SUB line */
    const uint32_t file = file_view(v);
    text_whole();
    uimn.text.hook = nullptr;

    for (uint32_t i = 0; i < n; i++)
        text_delete(S, first);

    touched();
    scan(S);
    views_moved(v, -1, file); /* the windows on it: its file's module */
    view_apply();
    uimn.ed.top = 0;
    edit_load(S, 0);
}

/* View v's item in the SUBs box: its file's name, a procedure's indented. */
static uint32_t item_len(Session *const S, const uint32_t v)
{
    return (uint32_t)strlen(vw.tab[v].kind ? view_name(v) : view_file(S, v)) +
           (vw.tab[v].kind ? 3 : 1); /* two spaces in; its 0 */
}

/* The SUBs box's list: each file's name, then its procedures' indented, by name; each item as wide
   as the longest. false: no memory. */
static bool list_fill(Session *const S)
{
    uint32_t size = 1;

    for (uint32_t i = 0; i < vw.n; i++) {
        const uint32_t len = item_len(S, i);
        size = len > size ? len : size;
    }

    if (!grow_room(&vw.list_text, &vw.list_cap, vw.n * size, 1) ||
        !GROW(vw.order, vw.order_cap, vw.n))
        return false;

    list.sel = 0;

    for (uint32_t i = 0; i < vw.n; i++) { /* insertion: a procedure among its file's, by name */
        uint32_t at = i;

        for (; vw.tab[i].kind && vw.tab[vw.order[at - 1]].kind &&
               dlog_namecmp(view_name(vw.order[at - 1]), view_name(i)) > 0;

             at--)
            vw.order[at] = vw.order[at - 1];

        vw.order[at] = i;
    }

    for (uint32_t k = 0; k < vw.n; k++) {
        const uint32_t v = vw.order[k];
        char *const d = vw.list_text + k * size;

        if (v == uimn.ed.view)
            list.sel = k;

        if (!vw.tab[v].kind) {
            copy_str(d, view_file(S, v));
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

const DlogList *view_list(Session *const S) { return list_fill(S) ? &list : nullptr; }

void view_subs(Session *const S)
{
    edit_commit(S);

    if (!list_fill(S))
        return;

    dg_subs.focus = 0;
    const uint8_t b = dlog_run(S, &dg_subs);
    const uint32_t v = list.n ? vw.order[list.sel] : 0;

    if (b == SUBS_ACTIVE) {
        show(S, v);
    } else if (b == SUBS_SPLIT) { /* the split's other window shows it */
        wind_other(S, false);
        show(S, v);
    } else if (b == SUBS_DELETE) {
        del(S, v);
    }
}
