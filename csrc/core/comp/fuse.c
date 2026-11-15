/* Op fusion: a statement's common sequence of ops runs as one.
 *
 * # How it works
 * comp_flush hands fuse_code the code it just made, before the statement map records its end:
 * jumps go to statements' starts only, so a statement's code can shrink inside. Operands are
 * still the compiler's (variable ids, Jumps unresolved); link_resolve resolves a fused op's.
 *
 *   NEXT   I.LD.V c, I.STEP.V s, I.ST.V c, I.LD.V c, I.CMP.V lim m, I.JF j   I.NEXT c s lim m j
 *
 * The fused ops have no asm counterpart; tests/golden compares the code unfused.
 */
#include "comp/fuse.h"
#include "vm/exec.h"
#include "vm/flow.h"

/* The NEXT sequence at p: its length, 0 not one. */
static uint32_t next_seq(const uint8_t *const p, const uint8_t *const end)
{
    static const uint8_t ops[] = {O_I_LD_V, O_I_STEP_V, O_I_ST_V, O_I_LD_V, O_I_CMP_V, O_I_JF};

    enum { LEN = 4 * (1 + VCELL) + 2 + VCELL + JSIZE };

    if (end - p < LEN)
        return 0;

    for (uint32_t i = 0, o = 0; i < countof(ops); o += flow_opsize(p + o), i++)
        if (p[o] != ops[i])
            return 0;

    const uint32_t c = load32(p + 1);
    return load32(p + 11) == c && load32(p + 16) == c ? LEN : 0;
}

uint8_t *fuse_code(uint8_t *const start, uint8_t *const end)
{
    uint8_t *out = start;

    for (const uint8_t *p = start; p < end;) {
        const uint32_t len = next_seq(p, end);

        if (len) { /* c s lim m j from the sequence's operands; out may be p */
            const uint8_t *const cmp = p + 4 * (1 + VCELL);
            out[0] = O_I_NEXT;
            store32(out + 1, load32(p + 1));
            store32(out + 5, load32(p + 6));
            memmove(out + 9, cmp + 1, VCELL + 1);
            memmove(out + 14, cmp + 2 + VCELL + 1, sizeof(Jump));
            out += NEXT_SIZE;
            p += len;
            continue;
        }

        const uint32_t n = flow_opsize(p);
        memmove(out, p, n);
        out += n;
        p += n;
    }

    return out;
}
