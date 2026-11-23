/* Palette rows and the nearest index (colour.h). */
#include "colour.h"
#include "mem.h"
#include "qbrt/llrm/qb/dev/host/vgadata.h"

/* A VGA DAC's 6 bits as 8. */
static uint32_t eight(const uint8_t six) { return (uint32_t)(six << 2 | six >> 4); }

void pal_default(Palette *const p)
{
    mem_fill(p, 0, sizeof *p);

    for (uint32_t i = 0; i < PAL_COLOURS; i++) {
        const unsigned char *const c = qb_vga_palette[i];
        p->rows[0][i] = eight(c[0]) << 16 | eight(c[1]) << 8 | eight(c[2]);
    }
}

static int32_t channel(const uint32_t rgb, const uint32_t shift)
{
    return (int32_t)(rgb >> shift & 0xFF);
}

uint8_t pal_nearest(const uint32_t *const row, const uint32_t rgb)
{
    uint8_t best = 0;
    int32_t least = INT32_MAX;

    for (uint32_t i = 0; i < PAL_COLOURS && least; i++) {
        const int32_t r = channel(row[i], 16) - channel(rgb, 16),
                      g = channel(row[i], 8) - channel(rgb, 8);
        const int32_t b = channel(row[i], 0) - channel(rgb, 0), d = r * r + g * g + b * b;

        if (d < least) {
            least = d;
            best = (uint8_t)i;
        }
    }

    return best;
}
