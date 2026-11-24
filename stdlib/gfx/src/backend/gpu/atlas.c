/* Atlas pages (backend/gpu/atlas.h). */
#include "backend/gpu/atlas.h"
#include "core/mem.h"

/* e at the page's next place, a row begun when it would not fit the last: false, no room. */
static bool fit(GpuPage *const page, GpuTexture *const e, const uint16_t w, const uint16_t h)
{
    if (page->x + w > GPU_PAGE) {
        page->x = 0;
        page->y += page->row;
        page->row = 0;
    }

    if (page->y + h > GPU_PAGE)
        return false;

    e->x = page->x;
    e->y = page->y;
    page->x += w;
    page->row = h > page->row ? h : page->row;
    page->live++;
    return true;
}

bool atlas_pack(
    Gpu *const g,
    GpuTexture *const e,
    const QxTextureFormat format,
    const uint16_t w,
    const uint16_t h
)
{
    for (uint32_t i = 0; i < g->npages; i++)
        if (g->pages[i].format == format && fit(&g->pages[i], e, w, h)) {
            e->page = GPU_PAGE_FIRST + i;
            return true;
        }

    const uint32_t made[] = {GPU_PAGE_FIRST + g->npages, GPU_PAGE, GPU_PAGE, format};

    if (!mem_grow((void **)&g->pages, &g->cappages, g->npages + 1, sizeof *g->pages) ||
        !list_op(&g->list, QX_LIST_TEXTURE, made, 4))
        return false;

    GpuPage *const page = &g->pages[g->npages++];
    *page = (GpuPage){.format = format};

    e->page = made[0];
    return fit(page, e, w, h);
}

void atlas_unpack(Gpu *const g, GpuTexture *const e)
{
    GpuPage *const page = &g->pages[e->page - GPU_PAGE_FIRST];

    if (!--page->live)
        *page = (GpuPage){.format = page->format};

    e->page = 0;
}
