/* GPU lists (qx/qxlist.h) played by the page's WebGL2, on this thread (web/gpu.mjs). A page
 * without one opens no screen, and gfx draws in pages.
 */
#include "plat/pgfx.h"
#include "qx/qx.h"
#include "wasm.h"

static bool player_open(const uint16_t w, const uint16_t h) { return js_gpu_open(w, h); }

static void player_read(
    const uint32_t texture,
    const int32_t x,
    const int32_t y,
    const int32_t w,
    const int32_t h,
    uint8_t *const out
)
{
    js_gpu_read(texture, x, y, w, h, out);
}

/* y down: GL's row 0 is a texture's first. */
static const PgfxPlayer player = {
    .shaders = 1 << QX_SHADER_ESSL,
    .y_down = true,
    .open = player_open,
    .close = js_gpu_close,
    .play = js_gpu_play,
    .read = player_read,
};

[[gnu::constructor]]
static void player_set(void)
{
    pgfx_set_player(&player);
}
