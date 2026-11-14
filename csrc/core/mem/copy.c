/* Copies in the arena and in the program store.
 *
 * # How it works
 * copy_mem moves bytes between two Addrs, overlapping allowed. copy_prog does the same between two
 * offsets in the program store, which a block move may relocate.
 *
 * # asm structure (src/mem/far.asm)
 * | asm | here | |
 * |---|---|---|
 * | mem_cpy, mem_up | copy_mem(dst, src, len) | MIRROR; D1 |
 * | mem_zero | copy_zero(a, len) | MIRROR; returns a + len |
 * | mem_pmov | copy_prog(dst, src, len) | MIRROR; D2 |
 * | mem_movs | memmove | MOVED: the C library |
 * | mem_line, mem_fs, mem_lin | | D1 |
 *
 * - D1. One memmove-based routine serves both directions; the unused identity routines are dropped.
 * - D2. Store offsets; ESI and EDI are not returned (src + len, dst + len).
 */
#include "mem/copy.h"
#include "session.h"
#include "mem/program.h"

void copy_mem(const Addr dst, const Addr src, const uint32_t len)
{
    memmove(at(dst), at(src), len);
}

Addr copy_zero(const Addr a, const uint32_t len)
{
    memset(at(a), 0, len);
    return a + len;
}

void copy_prog(Session *const S, const uint32_t dst, const uint32_t src, const uint32_t len)
{
    memmove(at(S->prog.base + dst), at(S->prog.base + src), len);
}
