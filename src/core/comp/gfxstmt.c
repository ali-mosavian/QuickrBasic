/* The screen's statements compiled: SCREEN, PCOPY, PSET, PRESET, LINE, CIRCLE, PAINT, DRAW, VIEW, WINDOW,
 * PALETTE, GET and PUT of the screen; CLS, COLOR, LOCATE, VIEW PRINT, WIDTH, BEEP.
 *
 * # How it works
 * A statement's operands are pushed, then an O_SYSCALL of its service (vm/syscalls.def). A point,
 * [STEP] (x, y), is its two coordinates as DOUBLEs and its own service, before the statement's. An
 * argument left out is -1, the runtime's "none"; SCREEN's are each a value and whether it was given.
 *
 * LINE's B and BF are names to the lexer: their text tells them. An array, name or name(i), is its
 * id, then its element's place and kind (expr_varaddr), 0 and 0 for its first. GET and PUT are the
 * screen's when a point follows, else the file's (the grammar's).
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | gstm_stmt | NEW |
 */
#include "comp/gfxstmt.h"
#include "comp/dfa.h"
#include "comp/expr.h"
#include "vm/exec.h"
#include "vm/syscall.h"

enum { SCREEN_ARGS = 4, COLOR_ARGS = 3, LOCATE_ARGS = 5, NONE = -1 };

static void gstm_op(Comp *const C, const Syscall n) { comp_byte(comp_op(C, O_SYSCALL), n); }

static void gstm_double(Comp *const C)
{
    expr_compile(C);
    comp_todouble(C);
}

static void gstm_int(Comp *const C)
{
    expr_compile(C);
    comp_toint(C);
}

/* An argument there: not at its ',' or the statement's end. */
static bool gstm_given(Comp *const C) { return comp_cur(C) != ',' && !comp_end(C); }

/* An optional INT argument: its value, or NONE. */
static void gstm_optint(Comp *const C)
{
    if (gstm_given(C))
        gstm_int(C);
    else
        comp_int(C, (uint32_t)NONE);
}

/* [STEP] (x, y): the point, as absolute's kind or, STEP, step's. */
static void gstm_point(Comp *const C, const Syscall absolute, const Syscall step)
{
    const bool stepped = comp_cur(C) == TK_STEP;
    C->p += stepped;
    comp_open(C);
    gstm_double(C);
    comp_comma(C);
    gstm_double(C);
    comp_close(C);
    gstm_op(C, stepped ? step : absolute);
}

/* Up to n optional INT arguments, comma separated, each a value and whether given, then service
   sc: SCREEN's, COLOR's, LOCATE's. */
static void gstm_block(Comp *const C, const uint32_t n, const Syscall sc)
{
    for (uint32_t i = 0; i < n; i++) {
        const bool given = (!i || comp_optcomma(C)) && gstm_given(C);

        if (given)
            gstm_int(C);
        else
            comp_int(C, 0);

        comp_int(C, given);
    }

    gstm_op(C, sc);
}

/* PSET or PRESET [STEP] (x, y) [, color]. */
static void gstm_pset(Comp *const C, const Syscall plain)
{
    gstm_point(C, SC_N1, SC_S1);

    if (!comp_optcomma(C)) {
        gstm_op(C, plain);
        return;
    }

    gstm_int(C);
    gstm_op(C, SC_PSETC);
}

/* LINE's B or BF at comp.p: 1, 2; 0 neither. */
static uint8_t gstm_linebox(Comp *const C)
{
    const uint8_t *const p = C->p;

    if (comp_cur(C) != M_VAR)
        return 0;

    const uint8_t n = p[1];
    const bool b = n && upcase(p[2]) == 'B';
    const uint8_t how = b && n == 1 ? 1 : b && n == 2 && upcase(p[3]) == 'F' ? 2 : 0;

    if (how)
        C->p = comp_skipname(p);

    return how;
}

/* LINE [[STEP] (x1, y1)] - [STEP] (x2, y2) [, [color] [, [B | BF] [, style]]]: without the first
   point, from the last. */
static void gstm_line(Comp *const C)
{
    if (comp_cur(C) == TK_MINUS)
        gstm_op(C, SC_LAST);
    else
        gstm_point(C, SC_N1, SC_S1);

    comp_need(C, TK_MINUS);
    gstm_point(C, SC_N2, SC_S2);
    uint8_t how = 0;

    if (comp_optcomma(C)) {
        gstm_optint(C);

        if (comp_optcomma(C)) {
            how = gstm_linebox(C);

            if (!how && gstm_given(C)) /* neither B nor BF */
                fault_error(C->fault, E_SN);

            comp_int(C, how);

            if (comp_optcomma(C))
                gstm_optint(C);
            else
                comp_int(C, (uint32_t)NONE);
        } else {
            comp_int(C, 0);
            comp_int(C, (uint32_t)NONE);
        }
    } else {
        comp_int(C, (uint32_t)NONE);
        comp_int(C, 0);
        comp_int(C, (uint32_t)NONE);
    }

    gstm_op(C, SC_LINE);
}

/* CIRCLE [STEP] (x, y), radius [, [color] [, [start] [, [end] [, aspect]]]]: the angles and the
   aspect, each given, set before it. */
static void gstm_circle(Comp *const C)
{
    static const Syscall sets[] = {SC_CSTART, SC_CEND, SC_CASPECT};

    gstm_point(C, SC_N1, SC_S1);
    comp_comma(C);
    gstm_double(C);

    if (comp_optcomma(C))
        gstm_optint(C);
    else
        comp_int(C, (uint32_t)NONE);

    for (uint32_t i = 0; i < countof(sets) && comp_optcomma(C); i++) {
        if (!gstm_given(C))
            continue;

        gstm_double(C);
        gstm_op(C, sets[i]);
    }

    gstm_op(C, SC_CIRCLE);
}

/* A string argument. */
static void gstm_str(Comp *const C)
{
    expr_compile(C);
    comp_tostring(C);
}

/* An array, name or name(subscripts): its id, its element's place and kind; 0 0 its first. */
static void gstm_array(Comp *const C)
{
    if (comp_cur(C) != M_VAR)
        fault_error(C->fault, E_SN);

    const uint8_t *const name = C->p;
    comp_int(C, comp_var(C, 2, 0));

    if (*C->p == '(') {
        C->p = name;
        expr_varaddr(C);
        return;
    }

    comp_int(C, 0);
    comp_int(C, 0);
}

/* PAINT [STEP] (x, y) [, [fill] [, [border] [, background$]]]: fill a colour, or a string a tile. */
static void gstm_paint(Comp *const C)
{
    gstm_point(C, SC_N1, SC_S1);
    bool tile = false;

    if (comp_optcomma(C) && gstm_given(C)) {
        expr_compile(C);
        tile = comp_type(C) & T_STR;

        if (!tile)
            comp_toint(C);
    } else
        comp_int(C, (uint32_t)NONE);

    if (comp_optcomma(C))
        gstm_optint(C);
    else
        comp_int(C, (uint32_t)NONE);

    if (!tile) {
        gstm_op(C, SC_PAINT);
        return;
    }

    const bool background = comp_optcomma(C) && gstm_given(C);

    if (background)
        gstm_str(C);

    comp_int(C, background);
    gstm_op(C, SC_PAINTT);
}

/* VIEW's or WINDOW's [SCREEN] (x1, y1)-(x2, y2), its corners by coord: whether SCREEN; -1 none,
   the statement alone. */
static int gstm_corners(Comp *const C, void (*const coord)(Comp *C))
{
    if (comp_end(C))
        return -1;

    const bool screen = comp_cur(C) == TK_X && C->p[1] == TK1_SCREEN;
    C->p += screen ? 2 : 0;

    for (uint32_t i = 0; i < 2; i++) {
        if (i)
            comp_need(C, TK_MINUS);

        comp_open(C);
        coord(C);
        comp_comma(C);
        coord(C);
        comp_close(C);
    }

    return screen;
}

/* VIEW [[SCREEN] (x1, y1)-(x2, y2) [, [fill] [, border]]]. */
static void gstm_view(Comp *const C)
{
    const int screen = gstm_corners(C, gstm_int);

    if (screen < 0) {
        gstm_op(C, SC_VIEW0);
        return;
    }

    for (uint32_t i = 0; i < 2; i++)
        if (comp_optcomma(C))
            gstm_optint(C);
        else
            comp_int(C, (uint32_t)NONE);

    comp_int(C, (uint32_t)screen);
    gstm_op(C, SC_VIEW);
}

/* WINDOW [[SCREEN] (x1, y1)-(x2, y2)]. */
static void gstm_window(Comp *const C)
{
    const int screen = gstm_corners(C, gstm_double);

    if (screen < 0) {
        gstm_op(C, SC_WINDOW0);
        return;
    }

    comp_int(C, (uint32_t)screen);
    gstm_op(C, SC_WINDOW);
}

/* PALETTE attribute, color, or PALETTE USING array[(i)]. */
static void gstm_palette(Comp *const C)
{
    if (comp_cur(C) == TK_X && C->p[1] == TK1_USING) {
        C->p += 2;
        gstm_array(C);
        gstm_op(C, SC_PALETTEU);
        return;
    }

    gstm_int(C);
    comp_comma(C);
    gstm_int(C);
    gstm_op(C, SC_PALETTE);
}

/* GET [STEP] (x1, y1)-[STEP] (x2, y2), array[(i)]. */
static void gstm_get(Comp *const C)
{
    gstm_point(C, SC_N1, SC_S1);
    comp_need(C, TK_MINUS);
    gstm_point(C, SC_N2, SC_S2);
    comp_comma(C);
    gstm_array(C);
    gstm_op(C, SC_GET);
}

/* PUT [STEP] (x, y), array[(i)] [, PSET | PRESET | AND | OR | XOR]: XOR without. */
static void gstm_put(Comp *const C)
{
    gstm_point(C, SC_N1, SC_S1);
    comp_comma(C);
    gstm_array(C);
    uint8_t how = 4;

    if (comp_optcomma(C)) {
        const uint8_t c = comp_cur(C);
        const uint8_t n = c == TK_X ? C->p[1] : 0;
        how = c == TK_OR ? 0 : c == TK_AND ? 1 : n == TK1_PRESET ? 2 : n == TK1_PSET ? 3 : 4;

        if (how == 4 && c != TK_XOR)
            fault_error(C->fault, E_SN);

        C->p += c == TK_X ? 2 : 1;
    }

    comp_int(C, how);
    gstm_op(C, SC_PUT);
}

/* After GET or PUT (comp.p): the screen's, a point following. */
static bool gstm_screens(const Comp *const C)
{
    const uint8_t *p = C->p;
    const uint8_t c = comp_peek(&p);
    return c == '(' || c == TK_STEP;
}

/* VIEW PRINT [top TO bottom]. */
static void gstm_viewprint(Comp *const C)
{
    if (comp_end(C)) {
        comp_int(C, (uint32_t)NONE);
        comp_int(C, (uint32_t)NONE);
    } else {
        gstm_int(C);
        comp_need(C, TK_TO);
        gstm_int(C);
    }

    gstm_op(C, SC_VIEWPRINT);
}

/* The text screen's one-byte statements: CLS [n], COLOR [..], LOCATE [..]; false not one. */
static bool gstm_text(Comp *const C)
{
    switch (C->p[0]) {
    case TK_CLS:
        C->p++;
        gstm_optint(C);
        gstm_op(C, SC_CLS);
        return true;

    case TK_COLOR:
        C->p++;
        gstm_block(C, COLOR_ARGS, SC_COLOR);
        return true;

    case TK_LOCATE:
        C->p++;
        gstm_block(C, LOCATE_ARGS, SC_LOCATE);
        return true;
    }

    return false;
}

bool gstm_stmt(Comp *const C)
{
    if (gstm_text(C))
        return true;

    if (C->p[0] == TK_GET) {
        C->p++;

        if (!gstm_screens(C)) {
            C->p--;
            return false;
        }

        gstm_get(C);
        return true;
    }

    if (C->p[0] != TK_X)
        return false;

    const uint8_t t = C->p[1];

    if (t == TK1_LINE && C->p[2] == TK_INPUT)
        return false;

    if (t == TK1_PUT) {
        C->p += 2;

        if (!gstm_screens(C)) {
            C->p -= 2;
            return false;
        }

        gstm_put(C);
        return true;
    }

    switch (t) {
    case TK1_SCREEN:
        C->p += 2;
        gstm_block(C, SCREEN_ARGS, SC_SCREEN);
        return true;

    case TK1_BEEP:
        C->p += 2;
        gstm_op(C, SC_BEEP);
        return true;

    case TK1_PCOPY:
        C->p += 2;
        gstm_int(C);
        comp_comma(C);
        gstm_int(C);
        gstm_op(C, SC_PCOPY);
        return true;

    case TK1_WIDTH:
        if (C->p[2] == '#' || C->p[2] == '"')
            return false; /* a file's or a device's */
        if (C->p[2] == TK_X && C->p[3] == TK1_LPRINT) {
            C->p += 4;
            gstm_int(C);
            comp_call(C, K_LPWID);
            return true;
        }

        C->p += 2;
        gstm_optint(C);

        if (comp_optcomma(C))
            gstm_int(C);
        else
            comp_int(C, (uint32_t)NONE);

        gstm_op(C, SC_WIDTH);
        return true;

    case TK1_PSET:
    case TK1_PRESET:
        C->p += 2;
        gstm_pset(C, t == TK1_PSET ? SC_PSET : SC_PRESET);
        return true;

    case TK1_LINE:
        C->p += 2;
        gstm_line(C);
        return true;

    case TK1_CIRCLE:
        C->p += 2;
        gstm_circle(C);
        return true;

    case TK1_PAINT:
        C->p += 2;
        gstm_paint(C);
        return true;

    case TK1_DRAW:
        C->p += 2;
        gstm_str(C);
        gstm_op(C, SC_DRAW);
        return true;

    case TK1_VIEW:
        C->p += 2;

        if (comp_cur(C) == TK_PRINT) {
            C->p++;
            gstm_viewprint(C);
            return true;
        }

        gstm_view(C);
        return true;

    case TK1_WINDOW:
        C->p += 2;
        gstm_window(C);
        return true;

    case TK1_PALETTE:
        C->p += 2;
        gstm_palette(C);
        return true;
    }

    return false;
}
