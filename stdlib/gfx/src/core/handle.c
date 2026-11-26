/* Handles: slots and generations (handle.h). */
#include "core/handle.h"
#include "core/mem.h"

enum { GEN_SHIFT = 20, SLOTS = HDL_SLOTS };

void hdl_init(HdlTable *const t, const uint8_t kind) { *t = (HdlTable){.kind = kind}; }

void hdl_done(HdlTable *const t)
{
    mem_free(t->gens);
    mem_free(t->items);
    hdl_init(t, t->kind);
}

/* need slots, slot 0 among them: false, no memory. */
static bool reserve(HdlTable *const t, const uint32_t need)
{
    uint32_t gens = t->cap, items = t->cap;

    if (need > SLOTS || !mem_grow((void **)&t->gens, &gens, need, 1) ||
        !mem_grow((void **)&t->items, &items, need, sizeof *t->items))
        return false;

    t->cap = gens < items ? gens : items;
    t->n = t->n ? t->n : 1;
    return true;
}

static uint32_t handle(const HdlTable *const t, const uint32_t slot)
{
    return hdl_owned(t->kind) | (uint32_t)t->gens[slot] << GEN_SHIFT | slot;
}

uint32_t hdl_add(HdlTable *const t, void *const item)
{
    uint32_t slot = 1;

    while (slot < t->n && t->items[slot])
        slot++;

    if (!reserve(t, slot + 1))
        return 0;

    t->n = slot + 1 > t->n ? slot + 1 : t->n;
    t->items[slot] = item;
    return handle(t, slot);
}

bool hdl_own(HdlTable *const t, void *const item)
{
    if (!reserve(t, 1))
        return false;

    t->items[0] = item;
    return true;
}

void *hdl_item(const HdlTable *const t, const uint32_t h)
{
    const uint32_t slot = hdl_slot(h);
    return slot < t->n && h == handle(t, slot) ? t->items[slot] : nullptr;
}

uint32_t hdl_at(const HdlTable *const t, const uint32_t slot)
{
    return slot < t->n && t->items[slot] ? handle(t, slot) : 0;
}

void hdl_drop(HdlTable *const t, const uint32_t h)
{
    if (!hdl_item(t, h))
        return;

    const uint32_t slot = hdl_slot(h);
    t->items[slot] = nullptr;

    if (slot)
        t->gens[slot]++;
}
