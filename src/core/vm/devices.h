/* The joystick's, the light pen's and the screen's services. How it works: devices.c. */
#ifndef CORE_VM_DEVICES_H
#define CORE_VM_DEVICES_H
#include "plat/pmou.h"

/* A mouse button as the light pen's switch: pressed since PEN(0) asked, and where it last was. */
typedef struct {
    bool pressed;
    PmouPress last;
} VdevButton;

typedef struct {
    VdevButton button[PMOU_NBUTTONS];
} Vdev;

/* CLR: no press, none waiting at the driver either (the editor's clicks). */
void vdev_clear(Vdev *dev);
#endif
