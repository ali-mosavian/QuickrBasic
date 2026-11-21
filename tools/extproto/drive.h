/* The VM's stand-in: how it works, drive.c. */
#ifndef DRIVE_H
#define DRIVE_H
#include "qx.h"

/* What the host's table gave back: a string result, the render audio_open was given. */
typedef struct {
    char text[64];
    void (QX_CALL *render)(int16_t *out, int32_t frames);
} DriveOut;

int drive(const QxModule *m, const QxHost *h, DriveOut *out);
#endif
