/* The joystick's, the light pen's and the screen's services (syscalls.def): STICK, STRIG, PEN
 * and SCREEN(r, c), llrm's (qbrt/qbrt.h); PEN the mouse's (pmou).
 *
 * # How it works
 * The mouse is the light pen, its left button the pen's switch: PEN(n), n 0 to 9, are QBasic's
 * (0 pressed since the last PEN(0), 1 2 the last press's x y, 3 down now, 4 5 x y now, 6 7 the last
 * press's row column, 8 9 now's). PEN(n + 10) is PEN(n) of the right button. The driver's presses
 * are taken at every PEN, so PEN(0) misses none between two. No mouse: nothing is pressed, at
 * 0, 0, row and column 1.
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | vdev_* | NEW |
 */
#include "session.h"
#include "qbrt/qbrt.h"

enum { PEN_LAST = 19, PEN_RIGHT = 10 };

void vdev_stick(Session *const S) { S->vm.sp->i = B_STIK((short)S->vm.sp->i); }
void vdev_strig(Session *const S) { S->vm.sp->i = B_FSTG((short)S->vm.sp->i); }

static void take(Vdev *const dev)
{
    for (uint8_t b = 0; b < PMOU_NBUTTONS; b++) {
        const PmouPress p = pmou_press(b);

        if (p.count) {
            dev->button[b].pressed = true;
            dev->button[b].last = p;
        }
    }
}

void vdev_clear(Vdev *const dev)
{
    if (pmou_there())
        take(dev);

    *dev = (Vdev){};
}

/* PEN(n) ( n -- i ). */
void vdev_pen(Session *const S)
{
    const int32_t n = S->vm.sp->i;

    if (n < 0 || n > PEN_LAST)
        repl_error(S, E_IQ);

    const bool there = pmou_there();
    const PmouState now = there ? pmou_state() : (PmouState){};

    if (there)
        take(&S->vdev);

    const uint8_t b = n >= PEN_RIGHT ? PMOU_RIGHT : PMOU_LEFT;
    VdevButton *const button = &S->vdev.button[b];
    const PmouPress *const last = &button->last;
    int32_t v = 0;

    switch (n % PEN_RIGHT) {
    case 0:
        v = button->pressed ? -1 : 0;
        button->pressed = false;
        break;

    case 1:
        v = last->x;
        break;

    case 2:
        v = last->y;
        break;

    case 3:
        v = now.buttons & 1u << b ? -1 : 0;
        break;

    case 4:
        v = now.x;
        break;

    case 5:
        v = now.y;
        break;

    case 6:
        v = last->row + 1;
        break;

    case 7:
        v = last->col + 1;
        break;

    case 8:
        v = now.row + 1;
        break;

    default:
        v = now.col + 1;
        break;
    }

    S->vm.sp->i = v;
}

void vdev_screen(Session *const S)
{
    const short f = exec_popi(&S->vm) == 3 ? (short)exec_popi(&S->vm) : 0;
    const short c = (short)exec_popi(&S->vm);
    S->vm.sp->i = B_FSCN((short)S->vm.sp->i, c, f);
}
