/* The joystick's and the light pen's services (syscalls.def): STICK, STRIG and PEN, llrm's
 * (qbrt/qbrt.h).
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
