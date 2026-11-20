/* A procedure's frame, laid out once its lines are compiled.
 *
 * # How it works
 * Each local gets a cell below the frame pointer in the order the locals were made, so a relink
 * leaves the old ones where a stopped call's frame has them. Below the cells, each local array a
 * DIM gave constant bounds (comp.c frame_shape) gets its header then its data, up to FRAME_ARRAYS
 * bytes; the others are the heap's. A parameter's Loc, its ref cell, is link_procvars'.
 */
#include "comp/frame.h"
#include "mem/array.h"

enum { FRAME_ARRAYS = 0x2000 };

void frame_layout(Symb *const sy, const Decl *const d, Proc *const p, const uint32_t proc)
{
    uint32_t cells = 0;

    for (uint32_t id = symb_first_local(sy, proc); id; id = symb_varx(sy, id)->nloc)
        cells += symb_is_local(sy, id);

    uint32_t k = cells; /* the list is newest first: the newest has the deepest cell */
    p->flags &= ~PF_ARRAYS;

    for (uint32_t id = symb_first_local(sy, proc); id; id = symb_varx(sy, id)->nloc) {
        if (!symb_is_local(sy, id))
            continue;

        *symb_loc(sy, id) = loc_frame(-(int32_t)(sizeof(Cell) * k--));

        if (symb_var(sy, id)->info & VI_ARR)
            p->flags |= PF_ARRAYS; /* its return frees them (vm/flow.c) */
    }

    uint32_t bytes = 0;

    for (uint32_t id = symb_first_local(sy, proc); id; id = symb_varx(sy, id)->nloc) {
        FrameArr *const fa = symb_farr(sy, id);
        fa->at = 0;

        if (!fa->shape || fa->shape == FARR_NEVER || !symb_is_local(sy, id))
            continue;

        const uint64_t data = (uint64_t)(fa->shape & 0xFFFFFF) * arry_elem_size(sy, d, id);
        const uint32_t size =
            arry_frame_hdr((uint8_t)(fa->shape >> 24)) + (uint32_t)((data + 7) & ~7ull);

        if (data > FRAME_ARRAYS || bytes + size > FRAME_ARRAYS)
            continue;

        bytes += size;
        cells += size / sizeof(Cell);
        fa->at = -(int32_t)(cells * sizeof(Cell));
    }

    p->nframe = (uint16_t)cells;
}
