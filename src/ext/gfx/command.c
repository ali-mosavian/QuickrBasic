/* Draw calls to commands, and the frame's log (command.h). */
#include "command.h"
#include "mem.h"

void cmd_place(Command *const c, const Pen *const pen, const Camera *const camera)
{
    c->target = pen->target;
    c->x -= camera->x;
    c->y -= camera->y;
    c->x2 -= camera->x;
    c->y2 -= camera->y;
    c->scale = 1;
    c->light = pen->light;
    c->tint = pen->tint;
    c->alpha = pen->alpha;
    c->recolor = pen->recolor;
}

bool cmd_log(CmdLog *const log, const Command *const c)
{
    if (!mem_grow((void **)&log->items, &log->cap, log->n + 1, sizeof *c))
        return false;

    log->items[log->n++] = *c;
    return true;
}

void cmd_clear(CmdLog *const log) { log->n = 0; }

void cmd_done(CmdLog *const log)
{
    mem_free(log->items);
    *log = (CmdLog){};
}
