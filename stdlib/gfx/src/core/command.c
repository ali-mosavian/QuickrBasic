/* Draw calls to commands, and the frame's log (command.h). */
#include "core/command.h"
#include "base/gmath.h"
#include "core/mem.h"

Place cmd_where(const Camera *const camera, const int32_t w, const int32_t h)
{
    if (!camera->centred)
        return (Place){
            .m11 = 1,
            .m22 = 1,
            .move = {-camera->x, -camera->y},
            .zoom = 1,
            .shifted = true,
        };

    float s, c;
    gm_sincos(camera->angle, &s, &c);
    const float z = camera->zoom;
    Place p = {
        .m11 = z * c,
        .m12 = z * s,
        .m21 = -z * s,
        .m22 = z * c,
        .angle = -camera->angle,
        .zoom = z,
    };

    /* the camera's point at the target's centre, between its middle pixels when even */
    const Vec at = cmd_at(&p, (Vec){camera->x, camera->y});

    p.move = (Vec){(float)(w - 1) / 2 - at.x, (float)(h - 1) / 2 - at.y};
    p.shifted = p.m11 == 1 && p.m12 == 0 && p.m21 == 0 && p.m22 == 1;

    return p;
}

void cmd_place(Command *const c, const Style *const style, const Place *const where)
{
    const Vec a = cmd_at(where, (Vec){c->x, c->y}), b = cmd_at(where, (Vec){c->x2, c->y2});

    c->x = a.x;
    c->y = a.y;
    c->x2 = b.x;
    c->y2 = b.y;
    c->angle += where->angle;
    c->scale *= where->zoom;
    c->light = style->light;
    c->tint = style->tint;
    c->alpha = style->alpha;
    c->palette_row = style->palette_row;
}

bool cmd_log(CmdLog *const log, const Command *const c)
{
    if (log->calls++, log->n == CMD_KEPT)
        return true;

    if (!mem_grow((void **)&log->items, &log->cap, log->n + 1, sizeof *c))
        return false;

    log->items[log->n++] = *c;
    return true;
}

void cmd_clear(CmdLog *const log) { log->n = log->calls = 0; }

void cmd_done(CmdLog *const log)
{
    mem_free(log->items);
    *log = (CmdLog){};
}
