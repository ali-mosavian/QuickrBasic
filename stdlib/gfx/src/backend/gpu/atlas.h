/* Atlas pages: a root drawn from, half a page or smaller, packed into a page of its format, so
 * sources share a texture and their sprites a batch. A page is packed in rows, a place freed with
 * its root, a page empty again started over.
 */
#ifndef GFX_GPU_ATLAS_H
#define GFX_GPU_ATLAS_H
#include "backend/gpu/gpu.h"

/* e, w by h, packed into a page of format, one made when none has room: false, no memory. */
bool atlas_pack(Gpu *g, GpuTexture *e, QxTextureFormat format, uint16_t w, uint16_t h);
/* e out of its page, which starts over when it empties. */
void atlas_unpack(Gpu *g, GpuTexture *e);
#endif
