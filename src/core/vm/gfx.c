/* The screen's services (syscalls.def), graphics and text: each pops its arguments and calls
 * llrm's QB runtime (qbrt/qbrt.h).
 *
 * # How it works
 * A statement's points come first, each its own service (SC_N1 ..), then the statement's. A
 * point's coordinates are DOUBLEs, given to the runtime as SINGLEs: it rounds them, or maps them
 * through WINDOW. SCREEN's arguments are each a value and whether it was given, made the runtime's
 * block. A string goes as the runtime's descriptor of its chars; an array as its id and the
 * element named (0 the first), the bytes from there to its end. A PAINT's queue is the BASIC heap's
 * free part, lent for the fill.
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | vgfx_* | NEW |
 */
#include "session.h"
#include "io/repl.h"
#include "mem/array.h"
#include "mem/string.h"
#include "mem/symbols.h"
#include "qbrt/qbrt.h"

enum { SCREEN_ARGS = 4, COLOR_ARGS = 3, LOCATE_ARGS = 5, BLOCK_ARGS = 5 };

/* ( x y -- ): two coordinates, y on top. */
static void vgfx_xy(Vm *const vm, float *const x, float *const y)
{
    *y = (float)exec_popd(vm);
    *x = (float)exec_popd(vm);
}

/* ( (value given) * n -- ) to the runtime's block: its count of words, then from the top down each
   argument's flag and, when given, its value. */
static void vgfx_block(Vm *const vm, const uint32_t n, int *const block)
{
    int given[BLOCK_ARGS], value[BLOCK_ARGS];

    for (uint32_t i = n; i--;) {
        given[i] = exec_popi(vm);
        value[i] = exec_popi(vm);
    }

    int words = 0;

    for (uint32_t i = 0; i < n; i++)
        words += given[i] ? 2 : 1;

    block[0] = words;
    int at = words;

    for (uint32_t i = 0; i < n; i++) {
        block[at--] = given[i];

        if (given[i])
            block[at--] = value[i];
    }
}

void vgfx_n1(Session *const S)
{
    float x, y;
    vgfx_xy(&S->vm, &x, &y);
    B_N1R4(x, y);
}

void vgfx_s1(Session *const S)
{
    float x, y;
    vgfx_xy(&S->vm, &x, &y);
    B_S1R4(x, y);
}

void vgfx_n2(Session *const S)
{
    float x, y;
    vgfx_xy(&S->vm, &x, &y);
    B_N2R4(x, y);
}

void vgfx_s2(Session *const S)
{
    float x, y;
    vgfx_xy(&S->vm, &x, &y);
    B_S2R4(x, y);
}

/* ( x y -- ) INTs: the runtime's INTEGER point where both fit one, else its SINGLE's, as a DOUBLE converted. */
static void vgfx_ixy(
    Vm *const vm,
    void (*const whole)(short, short),
    void (*const single)(float, float)
)
{
    const int32_t y = exec_popi(vm);
    const int32_t x = exec_popi(vm);

    if (x == (short)x && y == (short)y)
        whole((short)x, (short)y);
    else
        single((float)x, (float)y);
}

void vgfx_n1i(Session *const S) { vgfx_ixy(&S->vm, B_N1I2, B_N1R4); }
void vgfx_s1i(Session *const S) { vgfx_ixy(&S->vm, B_S1I2, B_S1R4); }
void vgfx_n2i(Session *const S) { vgfx_ixy(&S->vm, B_N2I2, B_N2R4); }
void vgfx_s2i(Session *const S) { vgfx_ixy(&S->vm, B_S2I2, B_S2R4); }

void vgfx_last(Session *const S) { B_S1R4(0, 0); }

void vgfx_screen(Session *const S)
{
    int block[1 + 2 * SCREEN_ARGS];
    vgfx_block(&S->vm, SCREEN_ARGS, block);
    screen_mode(block);
}

void vgfx_pcopy(Session *const S)
{
    const int to = exec_popi(&S->vm);
    gfx_pcopy(exec_popi(&S->vm), to);
}

void vgfx_pset(Session *const S) { B_PSET(); }

void vgfx_preset(Session *const S) { B_PRST(); }

void vgfx_psetc(Session *const S) { B_PSTC((short)exec_popi(&S->vm)); }

void vgfx_line(Session *const S)
{
    const short style = (short)exec_popi(&S->vm);
    const short how = (short)exec_popi(&S->vm);
    B_LINE((short)exec_popi(&S->vm), style, how);
}

void vgfx_cstart(Session *const S) { B_CSTT((float)exec_popd(&S->vm)); }

void vgfx_cend(Session *const S) { B_CSTO((float)exec_popd(&S->vm)); }

void vgfx_caspect(Session *const S) { B_CASP((float)exec_popd(&S->vm)); }

void vgfx_circle(Session *const S)
{
    const short color = (short)exec_popi(&S->vm);
    B_CIRC((float)exec_popd(&S->vm), color);
}

/* ( desc -- ) to the runtime's string. */
static QbrtStr vgfx_str(Session *const S)
{
    const Desc d = strg_get(&S->strg, exec_popa(&S->vm));
    return (QbrtStr){.len = d.len, .ptr = (char *)at(d.ptr)};
}

/* An array's part ( id element kind -- ): its chars from the element named (0 the first) to its
   end, their count, an element's bytes. Illegal function call: not made. */
typedef struct {
    uint8_t *data;
    uint32_t bytes, elem;
} Span;

static Span vgfx_array(Session *const S)
{
    (void)exec_popi(&S->vm); /* the element's kind */
    const Addr el = exec_popa(&S->vm);
    const uint16_t id = (uint16_t)exec_popi(&S->vm);
    const Addr hdr = arry_slot(&S->arry, id)->hdr;

    if (!hdr)
        repl_error(S, E_IQ);

    const Addr data = ((const ArrayHdr *)at(hdr))->data;
    const Addr end = data + arry_size(&S->arry, hdr);
    const Addr from = el ? el : data;

    if (from < data || from >= end)
        arry_bad_sub(&S->arry);

    return (Span){at(from), end - from, arry_esize(&S->arry, id)};
}

/* The BASIC heap's free part, the strings packed, lent to the PAINT that follows: before a string
   operand is read, since the packing moves strings. */
static void vgfx_lend(Session *const S)
{
    strg_gc(&S->strg);
    qbrt_lend(at(S->allo.arrays), S->allo.strings - S->allo.arrays);
}

void vgfx_paint(Session *const S)
{
    vgfx_lend(S);
    const short border = (short)exec_popi(&S->vm);
    B_PAIN((short)exec_popi(&S->vm), border);
}

void vgfx_paintt(Session *const S)
{
    vgfx_lend(S);

    if (exec_popi(&S->vm))
        (void)vgfx_str(S); /* the background tile: the runtime's PAINT takes none */

    const short border = (short)exec_popi(&S->vm);
    QbrtStr tile = vgfx_str(S);
    B_PNTC(&tile, border, -1);
}

void vgfx_draw(Session *const S)
{
    QbrtStr commands = vgfx_str(S);
    B_DRAW(&commands);
}

void vgfx_view(Session *const S)
{
    const short screen = (short)exec_popi(&S->vm);
    const short border = (short)exec_popi(&S->vm);
    const short fill = (short)exec_popi(&S->vm);
    const short y2 = (short)exec_popi(&S->vm);
    const short x2 = (short)exec_popi(&S->vm);
    const short y1 = (short)exec_popi(&S->vm);
    B_VIEW((short)exec_popi(&S->vm), y1, x2, y2, fill, border, screen);
}

void vgfx_view0(Session *const S) { B_VEW0(); }

void vgfx_window(Session *const S)
{
    const short screen = (short)exec_popi(&S->vm);
    float x1, y1, x2, y2;
    vgfx_xy(&S->vm, &x2, &y2);
    vgfx_xy(&S->vm, &x1, &y1);
    B_WIND(x1, y1, x2, y2, screen);
}

void vgfx_window0(Session *const S) { B_WIN0(); }

void vgfx_palette(Session *const S)
{
    const long color = exec_popi(&S->vm);
    B_PAL2((short)exec_popi(&S->vm), color);
}

void vgfx_paletteu(Session *const S)
{
    const Span a = vgfx_array(S);
    qbrt_palette_using(a.data, a.bytes, a.elem);
}

void vgfx_get(Session *const S)
{
    const Span a = vgfx_array(S);
    qbrt_get(a.data, a.bytes, a.elem);
}

void vgfx_put(Session *const S)
{
    const short how = (short)exec_popi(&S->vm);
    const Span a = vgfx_array(S);
    qbrt_put(a.data, a.bytes, a.elem, how);
}

void vgfx_point(Session *const S)
{
    const float y = exec_pop(&S->vm).f;
    const float x = exec_pop(&S->vm).f;
    exec_pushi(&S->vm, B_PNR4(x, y));
}

void vgfx_pmap(Session *const S)
{
    const short function = (short)exec_popi(&S->vm);
    const float value = exec_pop(&S->vm).f;
    exec_push(&S->vm, (Cell){.f = *B_PMAP(value, function)});
}

/* After the runtime moved the cursor: the console's column its. */
static void vgfx_column(Session *const S)
{
    if (qbrt_screen())
        S->cons.col = cn_pos();
}

void vgfx_cls(Session *const S)
{
    B_SCLS((short)exec_popi(&S->vm));
    vgfx_column(S);
}

void vgfx_color(Session *const S)
{
    int block[1 + 2 * COLOR_ARGS];
    vgfx_block(&S->vm, COLOR_ARGS, block);
    screen_color(block);
}

void vgfx_locate(Session *const S)
{
    int block[1 + 2 * LOCATE_ARGS];
    vgfx_block(&S->vm, LOCATE_ARGS, block);
    screen_locate(block);
    vgfx_column(S);
}

void vgfx_viewprint(Session *const S)
{
    const short bottom = (short)exec_popi(&S->vm);
    B_VWPT((short)exec_popi(&S->vm), bottom);
    vgfx_column(S);
}

void vgfx_width(Session *const S)
{
    const short rows = (short)exec_popi(&S->vm);
    B_WIDT((short)exec_popi(&S->vm), rows);
    vgfx_column(S);
}

void vgfx_beep(Session *const S) { B_BEEP(); }

void vgfx_csrlin(Session *const S) { exec_pushi(&S->vm, qbrt_screen() ? cn_line() + 1 : 1); }
