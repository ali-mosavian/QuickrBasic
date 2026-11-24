/* Handles: a long naming an item, so a freed or wrong one finds nothing, never a stray pointer.
 *
 * # How it works
 * A handle is a kind (4 bits), a generation (8) and a slot (20); 0 is none. A table holds one
 * kind's items by slot, and each slot's generation: dropping an item bumps it, so the old handle no
 * longer matches, and the next add takes the slot again. Slot 0 is the kind's own (the screen's
 * surface): its generation stays 0, so its handle is a constant (hdl_owned).
 */
#ifndef GFX_HANDLE_H
#define GFX_HANDLE_H
#include <stdint.h>

enum { HDL_SURFACE = 1, HDL_FONT = 2, HDL_SLOTS = 1 << 20, HDL_KIND_SHIFT = 28 };

typedef struct {
    uint8_t kind;
    uint32_t n, cap; /* the slots used (slot 0 always), and allocated */
    uint8_t *gens;
    void **items; /* null a free slot */
} HdlTable;

void hdl_init(HdlTable *t, uint8_t kind);
/* The table's memory; its items are the owner's to free first. */
void hdl_done(HdlTable *t);
/* item in a free slot: its handle, 0 no memory. */
uint32_t hdl_add(HdlTable *t, void *item);
/* item in slot 0 (null: none): false, no memory. */
bool hdl_own(HdlTable *t, void *item);
/* h's item: null, none (freed, or another kind's). */
void *hdl_item(const HdlTable *t, uint32_t h);
/* The handle of the item in slot, for walking the table: 0 none there. */
uint32_t hdl_at(const HdlTable *t, uint32_t slot);
void hdl_drop(HdlTable *t, uint32_t h);

static inline uint32_t hdl_slot(const uint32_t h) { return h & (HDL_SLOTS - 1); }

/* The handle of a kind's slot 0. */
static inline uint32_t hdl_owned(const uint8_t kind) { return (uint32_t)kind << HDL_KIND_SHIFT; }
#endif
