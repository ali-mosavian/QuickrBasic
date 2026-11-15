/* The graphics services (syscalls.def): each pops its arguments and calls llrm's QB runtime
 * (qbrt/qbrt.h).
 *
 * # How it works
 * A statement's points come first, each its own service (SC_N1 ..), then the statement's. A
 * point's coordinates are DOUBLEs, given to the runtime as SINGLEs: it rounds them, or maps them
 * through WINDOW. SCREEN's arguments are each a value and whether it was given, made the runtime's
 * block.
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | vgfx_* | NEW |
 */
#include "session.h"
#include "qbrt/qbrt.h"

enum { SCREEN_ARGS = 4 };

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
    int given[SCREEN_ARGS], value[SCREEN_ARGS];

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

void vgfx_last(Session *const S) { B_S1R4(0, 0); }

void vgfx_screen(Session *const S)
{
    int block[1 + 2 * SCREEN_ARGS];
    vgfx_block(&S->vm, SCREEN_ARGS, block);
    screen_mode(block);
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
