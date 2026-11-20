/* Each op's addressing mode, picked from its operands' Locs once the frames are laid out.
 *
 * # How it works
 * Codegen names a variable by its id. Here an op on a variable in a frame becomes its frame twin
 * (ops.def FRAME), its operand the offset from the frame; any other gets its cell. A PRET gets the
 * form its procedure's result and arrays need. comp/link.c then resolves jumps and procedures.
 */
#include "comp/select.h"
#include "vm/exec.h"

/* Each op's frame twin; 0 none. */
static const uint8_t twins[256] = {
#define FRAME(op, twin) [O_##op] = O_##twin,
#include "vm/ops.def"
};

static uint8_t twin(const uint8_t op) { return twins[op]; }

/* An op on locals, n cells of them: its twin, the offset of each cell. False: one not in a frame, or
   an op with no twin (comp/fuse.c fuses locals into the ones that have). */
static bool local_op(const Symb *const sy, uint8_t *const op, const uint32_t n)
{
    const uint8_t t = twin(op[0]);

    if (!t)
        return false;

    for (uint32_t i = 0; i < n; i++)
        if (!loc_in_frame(*symb_loc(sy, load32(op + 1 + i * VCELL))))
            return false;

    for (uint32_t i = 0; i < n; i++) {
        uint8_t *const cell = op + 1 + i * VCELL;
        store32(cell, (uint32_t)loc_off(*symb_loc(sy, load32(cell))));
    }

    op[0] = t;
    return true;
}

/* A 1-D array op on an array in its frame: its twin, the header's offset. False: not in a frame. */
static bool frame_op(const Symb *const sy, uint8_t *const op)
{
    const FrameArr *const fa = symb_farr(sy, load32(op + 1));
    const uint8_t t = twin(op[0]);

    if (!fa->at || fa->shape >> 24 != 1 || !t)
        return false;

    op[0] = t;
    store32(op + 1 + VID, (uint32_t)fa->at);
    return true;
}

/* PRET for its procedure: a SUB's; a FUNCTION's, its result's cell or offset in the frame; a string
   to keep: PRET.X. */
static void select_pret(const Symb *const sy, const Blok *const b, uint8_t *const op)
{
    const Proc *const p = blok_proc(b, load32(op + 1));
    const uint32_t res = p->result;

    if (res && (symb_var(sy, res)->info & VK_MASK) == VK_STR)
        op[0] = O_PRET_X;
    else if (res && symb_in_frame(sy, res)) {
        op[0] = O_PRET_FL;
        store32(op + 5, (uint32_t)loc_off(*symb_loc(sy, res)));
    } else if (res) {
        op[0] = O_PRET_FV;
        store32(op + 5, symb_cell(sy, res));
    }
}

void select_op(const Symb *const sy, const Blok *const b, uint8_t *const op)
{
    static const uint8_t cells[] = {FV, FV2, FV3};

    const uint8_t fl = exec_flags(op[0]);

    if (fl & FV && local_op(sy, op, 1 + !!(fl & FV2) + !!(fl & FV3)))
        ;
    else
        for (uint32_t i = 0; i < countof(cells) && fl & cells[i]; i++)
            store32(op + 1 + i * VCELL, symb_cell(sy, load32(op + 1 + i * VCELL)));

    if (fl & FA && !frame_op(sy, op)) /* an array's or a record's cell */
        store32(op + 1 + VID, (uint32_t)symb_placed(sy, load32(op + 1)).w);
    if (fl & FP) /* a parameter: its ref cell in the frame */
        store32(op + 1, (uint32_t)loc_off(*symb_loc(sy, load32(op + 1))));
    if (op[0] == O_PRET)
        select_pret(sy, b, op);
}
