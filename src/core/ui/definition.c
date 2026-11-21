/* Go to Definition, ours: Ctrl+B, or Ctrl+click, on a name shows the line that defines it.
 *
 * # How it works
 * The word at the cursor is a name of its file's module. One an IMPORT or FROM binds stands for the
 * exporter's (symb_alias); a module's name for its file. A SUB's or FUNCTION's definition is its
 * first line. Else it is the first line that declares the name, in the cursor's procedure, then in
 * its file's module lines: DIM, REDIM, STATIC, CONST, COMMON, TYPE, a parameter, DEF FN, a label.
 * Else the first line there that names it. `p.x` with none is `p`'s. A line number's is its line.
 * Ctrl+Alt shows it in the split's other window, split side by side first when there is none.
 */
#include "ui/definition.h"
#include "ui/jumps.h"
#include "session.h"
#include "comp/bind.h"
#include "comp/comp.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "plat/pscr.h"
#include "ui/edit.h"
#include "ui/linemap.h"
#include "ui/uimain.h"
#include "ui/view.h"
#include "ui/window.h"

enum : uint32_t { NONE = 0xFFFFFFFF };

/* A name as its definition's: its module, its id there. */
typedef struct {
    uint32_t module, name;
} Target;

/* c ends a statement's tokens. */
static bool ends_stmt(const uint8_t c)
{
    return !c || c == ':' || c == TK_REM || c == TK_DATA || c == TK_ELSE;
}

static bool is_x(const uint8_t *const p, const uint8_t k) { return p[0] == TK_X && p[1] == k; }

/* Past the list item at p: at its ',' or ')' outside parentheses, or its statement's end. */
static const uint8_t *item_end(const uint8_t *p)
{
    for (uint32_t depth = 0; !ends_stmt(*p); p = decl_item(p)) {
        if (!depth && (*p == ',' || *p == ')'))
            break;
        if (*p == '(')
            depth++;
        else if (*p == ')')
            depth--;
    }

    return p;
}

/* The list at p, items apart by ',' (DIM's, CONST's, COMMON's, parameters): one starts with name
   t, past its BYVAL, SHARED, PRESERVE, PUBLIC or COMMON's /block/. */
static bool lists(const uint8_t *p, const uint32_t t)
{
    for (;;) {
        while (*p == TK_BYVAL || *p == TK_SHARED || is_x(p, TK1_PRESERVE) || is_x(p, TK1_PUBLIC))
            p = decl_item(p);

        if (*p == '/') {
            for (p++; !ends_stmt(*p) && *p != '/';)
                p = decl_item(p);

            p += *p == '/';
        }

        if (*p == M_VAR && comp_nameof(p) == t)
            return true;

        p = item_end(p);

        if (*p != ',')
            return false;

        p++;
    }
}

/* The name at p, a procedure's or DEF FN's, or one of its ( parameters ), is t. */
static bool header(const uint8_t *p, const uint32_t t)
{
    if (*p != M_VAR)
        return false;
    if (comp_nameof(p) == t)
        return true;

    p = comp_skipname(p);
    return *p == '(' && lists(p + 1, t);
}

/* The statement at p declares t. */
static bool stmt_declares(const uint8_t *p, const uint32_t t)
{
    if (is_x(p, TK1_PRIVATE))
        p += 2;

    switch (*p) {
    case TK_DIM:
    case TK_REDIM:
    case TK_STATIC:
    case TK_CONST:
        return lists(p + 1, t);

    case TK_TYPE:
        return p[1] == M_VAR && comp_nameof(p + 1) == t;

    case TK_SUB:
    case TK_FUNCTION:
    case TK_DEF:
        return header(p + 1, t);

    case TK_X:
        return p[1] == TK1_COMMON && lists(p + 2, t);

    default:
        return false;
    }
}

/* Body p is t's label, or one of its statements declares t. */
static bool declares(const uint8_t *p, const uint32_t t)
{
    if (*p == M_INT)
        p += INT_TOKEN_SIZE;

    if (*p == M_VAR && *comp_skipname(p) == ':') {
        if (comp_nameof(p) == t)
            return true;

        p = comp_skipname(p) + 1;
    }

    for (;;) {
        if (stmt_declares(p, t))
            return true;

        while (!ends_stmt(*p))
            p = decl_item(p);

        if (*p != ':')
            return false;

        p++;
    }
}

/* Body p names t. */
static bool names(const uint8_t *p, const uint32_t t)
{
    for (; *p && *p != TK_REM && *p != TK_DATA; p = decl_item(p))
        if (*p == M_VAR && comp_nameof(p) == t)
            return true;

    return false;
}

/* Record rec's line: its body declares t, names t, has the number t. */
typedef bool LinePred(Session *S, uint32_t rec, uint32_t t);

static bool line_declares(Session *const S, const uint32_t rec, const uint32_t t)
{
    prog_body(&S->prog, rec, S->comp.cbuf);
    return declares(S->comp.cbuf, t);
}

static bool line_names(Session *const S, const uint32_t rec, const uint32_t t)
{
    prog_body(&S->prog, rec, S->comp.cbuf);
    return names(S->comp.cbuf, t);
}

static bool line_numbered(Session *const S, const uint32_t rec, const uint32_t t)
{
    return prog_line(&S->prog, rec)->num == t;
}

/* View v's first text line pred holds for; NONE none. */
static uint32_t find_in(Session *const S, const uint32_t v, LinePred *const pred, const uint32_t t)
{
    const View *const w = view_at(v);

    for (uint32_t line = w->first; line < w->first + w->n; line++)
        if (pred(S, lmap_off(S, line), t))
            return line;

    return NONE;
}

/* The first text line pred holds for in the cursor's procedure when it is module's, then in
   module's own lines; NONE none. */
static uint32_t look(
    Session *const S,
    const uint32_t module,
    LinePred *const pred,
    const uint32_t t
)
{
    const uint32_t here = uimn.ed.view, own = view_of_module(module);

    if (here != own && view_module(here) == module) {
        const uint32_t line = find_in(S, here, pred, t);

        if (line != NONE)
            return line;
    }

    return find_in(S, own, pred, t);
}

/* Name n of module m as its definition's: the exporter's when an import binds it. */
static Target resolve(const Symb *const sy, const uint32_t m, const uint32_t n)
{
    const uint32_t a = *symb_alias(sy, n);

    if (!a || symb_name(sy, n)->flags & (NM_MOD | NM_BAD))
        return (Target){m, n};

    const uint32_t t = a & ~BIND_STAR;
    return (Target){symb_name(sy, t)->module, t};
}

/* A definition found: its text line (NONE none), the name the cursor goes on (len bytes; 0 none). */
typedef struct {
    uint32_t line;
    const uint8_t *name;
    uint8_t len;
} Found;

/* t's defining text line: its procedure's first, else the first that declares it; NONE none. */
static uint32_t defining(Session *const S, const Target t)
{
    const uint32_t v = view_procedure(S, t.module, t.name);
    return v ? view_at(v)->first : look(S, t.module, line_declares, t.name);
}

/* f's line shown, the cursor on its first word spelled as f's name (its suffix aside), else at its
   start. */
static void jump(Session *const S, const Found f)
{
    edit_load(S, view_goline(S, f.line));
    const LineBuf *const b = &uimn.edline;
    const uint8_t *const name = f.name, len = f.len;
    uint16_t col = 0;

    for (uint16_t i = 0; len && i + len <= b->len; i++) {
        const bool starts = !i || !is_name_char(b->text[i - 1]);
        const bool ends = i + len == b->len || !is_name_char(b->text[i + len]);
        uint8_t k = 0;

        while (k < len && upcase(b->text[i + k]) == upcase(name[k]))
            k++;

        if (starts && ends && k == len) {
            col = i;
            break;
        }
    }

    uimn.edline.col = col;
    uimn.ed.sel_on = false;
    edit_ensure();
}

/* The word's digits as a line number; NONE not one. */
static uint32_t line_number(const char *const w, const uint16_t n)
{
    uint32_t num = 0;

    for (uint16_t i = 0; i < n; i++) {
        if (w[i] < '0' || w[i] > '9' || num > NONUM)
            return NONE;

        num = num * 10 + (uint32_t)(w[i] - '0');
    }

    return num < NONUM ? num : NONE;
}

/* The definition of the name at the cursor. */
static Found find(Session *const S)
{
    char w[UINT8_MAX + 1];
    uint16_t n = edit_word(w, sizeof w);

    if (!n || !edit_commit(S))
        return (Found){.line = NONE};

    const Symb *const sy = &S->symb;
    const uint32_t m = view_module(uimn.ed.view), num = line_number(w, n);

    if (num != NONE)
        return (Found){.line = look(S, m, line_numbered, num)};

    n -= memchr("$%&!#", w[n - 1], 5) != nullptr; /* a suffix: the name's type */
    const char *const dot = memchr(w, '.', n);
    const uint32_t whole = symb_look_name(sy, m, (const uint8_t *)w, (uint8_t)n);
    const uint32_t head = dot ? symb_look_name(sy, m, (const uint8_t *)w, (uint8_t)(dot - w)) : 0;

    if (whole && symb_name(sy, whole)->flags & NM_MOD)
        return (Found){.line = view_at(view_of_module(*symb_alias(sy, whole)))->first};

    const uint32_t ids[] = {whole, head};

    Target ts[countof(ids)] = {};

    for (uint32_t i = 0; i < countof(ids); i++)
        if (ids[i])
            ts[i] = resolve(sy, m, ids[i]);

    for (uint32_t pass = 0; pass < 2; pass++) { /* a definition, then a line naming it */
        for (uint32_t i = 0; i < countof(ids); i++) {
            const uint32_t line = !ids[i] ? NONE
                                  : pass  ? look(S, ts[i].module, line_names, ts[i].name)
                                          : defining(S, ts[i]);

            if (line != NONE) {
                const uint8_t *const text = symb_text(sy, ts[i].name);
                return (Found){line, text + 1, text[0]};
            }
        }
    }

    return (Found){.line = NONE};
}

void defn_goto(Session *const S, const bool beside)
{
    const Found f = find(S);

    if (f.line == NONE)
        return pscr_beep();
    if (beside)
        wind_other(S, true);
    else
        jumps_push();

    jump(S, f);
}
