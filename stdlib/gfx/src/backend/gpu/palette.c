/* The palette on the GPU (backend/gpu/gpu.h's gpu_palette): its rows, then row 0's map. */
#include "backend/gpu/gpu.h"
#include "core/mem.h"

enum { RGBA = 4, OPAQUE = 0xFF };

/* The palette's rows sent: false, no memory. */
static bool send_rows(Gpu *const g, const Palette *const pal)
{
    uint8_t *const rows = list_keep(&g->list, PAL_ROWS * PAL_COLOURS * RGBA);
    uint32_t args[8] = {GPU_PALETTE, 0, 0, PAL_COLOURS, PAL_ROWS, PAL_COLOURS * RGBA};

    if (!rows)
        return false;

    for (uint32_t r = 0; r < PAL_ROWS; r++)
        for (uint32_t i = 0; i < PAL_COLOURS; i++) {
            const uint32_t c = pal->rows[r][i];
            mem_copy(
                rows + (r * PAL_COLOURS + i) * RGBA,
                (const uint8_t[]){c >> 16, c >> 8, c, OPAQUE},
                RGBA
            );
        }

    list_pointer(args + 6, rows);
    return list_op(&g->list, QX_LIST_UPLOAD, args, 8);
}

/* Row 0's map sent, below the rows: false, no memory. */
static bool send_map(Gpu *const g, Palette *const pal)
{
    const uint8_t *const map = pal_map(pal);
    uint8_t *const cells = map ? list_keep(&g->list, PAL_CELLS) : nullptr;
    uint32_t args[8] = {GPU_PALETTE, 0, PAL_ROWS, PAL_COLOURS, GPU_MAP_ROWS, PAL_COLOURS * RGBA};

    if (!cells)
        return false;

    mem_copy(cells, map, PAL_CELLS);
    list_pointer(args + 6, cells);
    return list_op(&g->list, QX_LIST_UPLOAD, args, 8);
}

bool gpu_palette(Gpu *const g, Palette *const pal, const bool map)
{
    if (g->fixed) /* its texels carry their colours */
        return true;

    const uint32_t row0 = pal->changed[0] + 1;
    uint32_t changes = 1;

    for (uint32_t r = 0; r < PAL_ROWS; r++)
        changes += pal->changed[r];

    if (changes != g->palette && !send_rows(g, pal))
        return false;

    g->palette = changes;
    g->row0 = row0;

    if (!map || row0 == g->mapped)
        return true;

    g->mapped = send_map(g, pal) ? row0 : 0;
    return g->mapped;
}
