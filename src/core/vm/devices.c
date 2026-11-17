/* The joystick's, the light pen's and the screen's services (syscalls.def): STICK, STRIG, PEN
 * and SCREEN(r, c), llrm's (qbrt/qbrt.h).
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | vdev_* | NEW |
 */
#include "session.h"
#include "qbrt/qbrt.h"

void vdev_stick(Session *const S) { S->vm.sp->i = B_STIK((short)S->vm.sp->i); }
void vdev_strig(Session *const S) { S->vm.sp->i = B_FSTG((short)S->vm.sp->i); }
void vdev_pen(Session *const S) { S->vm.sp->i = B_FPEN((short)S->vm.sp->i); }

void vdev_screen(Session *const S)
{
    const short f = exec_popi(&S->vm) == 3 ? (short)exec_popi(&S->vm) : 0;
    const short c = (short)exec_popi(&S->vm);
    S->vm.sp->i = B_FSCN((short)S->vm.sp->i, c, f);
}
