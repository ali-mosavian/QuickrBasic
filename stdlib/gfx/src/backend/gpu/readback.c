/* What the GPU drew, read back (backend/gpu/gpu.h's gpu_sync), and a root's texture found. */
#include "backend/gpu/gpu.h"
#include "gfx_qx.h"
#include "core/mem.h"

enum { RGBA = 4, HALF = 0x80 };

GpuTexture *gpu_made(Gpu *const g, const uint32_t h)
{
    const uint32_t slot = hdl_slot(h);
    GpuTexture *const e = slot < g->ntextures ? &g->textures[slot] : nullptr;

    return e && e->handle == h ? e : nullptr;
}

bool gpu_sync(Gpu *const g, const Pixels px, const uint32_t h)
{
    GpuTexture *const e = gpu_made(g, h);

    if (!e || !e->drawn)
        return true;

    gpu_play(g, false);
    uint8_t *const rgba = mem_alloc((uint32_t)(px.w * px.h) * RGBA);

    if (!rgba)
        return false;

    qx_host->gpu_read(h, 0, 0, px.w, px.h, rgba);
    g->count.waits++;
    const uint32_t bytes = px_bytes(px.format);

    for (int32_t y = 0; y < px.h; y++)
        for (int32_t x = 0; x < px.w; x++) {
            const uint8_t *const c = rgba + (y * px.w + x) * RGBA;
            const uint32_t rgb = (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2];

            const uint32_t v = px.format == PX_8    ? c[3]
                               : px.format == PX_32 ? (uint32_t)c[3] << 24 | rgb
                               : c[3] < HALF        ? 0
                                                    : px_pack(px.format, rgb);
            px_store(px.p + y * px.pitch + (uint32_t)x * bytes, v, bytes);
        }

    mem_free(rgba);
    e->drawn = false;
    return true;
}
