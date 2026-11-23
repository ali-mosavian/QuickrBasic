/* Commands: each draw call as it was drawn, after the camera, with the pen's numbers; the frame's
 * log of them, which display.drawn reads: the same on every backend, so tests assert on it.
 */
#ifndef GFX_COMMAND_H
#define GFX_COMMAND_H
#include "pen.h"

/* x, y the anchor or the first end; x2, y2 the other end. kind is display's DREW_*. */
typedef struct {
    int32_t kind;
    uint32_t target, source;
    float x, y, x2, y2, angle, scale;
    int32_t flags, colour, light;
    uint32_t tint;
    int32_t alpha, recolor;
} Command;

/* draw.camera's: the world's point at the target's top left. */
typedef struct {
    float x, y;
} Camera;

typedef struct {
    Command *items;
    uint32_t n, cap;
} CmdLog;

/* A draw call's command placed: its points moved by the camera, the pen's target and numbers. */
void cmd_place(Command *c, const Pen *pen, const Camera *camera);
/* c logged: false, no memory. */
bool cmd_log(CmdLog *log, const Command *c);
/* A frame begins. */
void cmd_clear(CmdLog *log);
void cmd_done(CmdLog *log);
#endif
