/* Where a variable's cell is: a cell's address (positive); in the frame (negative), at an offset
 * from it (bit 2 clear: cells are 8-aligned) or through a ref cell holding its address (bit 2 set,
 * the ref cell's offset k as -k - 4). The symbol table keeps one per variable (symbols.h
 * symb_loc), 0 its own cell; an FA op its array's (link). One decoder, loc_at, reads both.
 */
#ifndef MEM_LOC_H
#define MEM_LOC_H
#include "base.h"

typedef struct {
    int32_t w;
} Loc;

/* A ref cell's address, tagged when its target is narrower than a cell: REF_NARROW, and its ET_*
   less ET_SH at bit REF_KIND. Addresses stay under REF_ADDR. */
enum : uint32_t { REF_NARROW = 0x80000000u, REF_KIND = 29, REF_ADDR = 0x1FFFFFFFu };

static inline Loc loc_cell(const Addr a) { return (Loc){(int32_t)a}; }
static inline Loc loc_frame(const int32_t off) { return (Loc){off}; }
static inline Loc loc_ref(const uint32_t off) { return (Loc){-(int32_t)off - 4}; }
/* A local before its frame is laid out (comp/frame.c). */
static inline Loc loc_local(void) { return (Loc){-8}; }

/* In the frame: a local's or a parameter's. */
static inline bool loc_in_frame(const Loc p) { return p.w < 0; }
static inline bool loc_is_ref(const Loc p) { return ((uint32_t)p.w & 0x80000004u) == 0x80000004u; }
static inline bool loc_is_local(const Loc p)
{
    return ((uint32_t)p.w & 0x80000004u) == 0x80000000u;
}
/* An in-frame Loc's offset from the frame: its cell's, or its ref cell's. */
static inline int32_t loc_off(const Loc p) { return p.w & 4 ? -p.w - 4 : p.w; }

/* The cell an in-frame p names, in the frame at fp. */
static inline Addr loc_frame_at(const Loc p, const Addr fp)
{
    return p.w & 4 ? rd32(fp + (uint32_t)(-p.w - 4)) & REF_ADDR : fp + (uint32_t)p.w;
}

/* The cell p names, in the frame at fp. */
static inline Addr loc_at(const Loc p, const Addr fp)
{
    return p.w > 0 ? (Addr)p.w : loc_frame_at(p, fp);
}
#endif
