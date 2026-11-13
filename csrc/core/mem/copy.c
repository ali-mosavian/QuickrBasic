#include "mem/copy.h"
#include "mem/prog.h"

void copy_mem(Addr dst, Addr src, uint32_t len) { memmove(at(dst), at(src), len); }

Addr copy_zero(Addr a, uint32_t len)
{
    memset(at(a), 0, len);
    return a + len;
}

void copy_prog(uint32_t dst, uint32_t src, uint32_t len)
{
    memmove(at(prog.base + dst), at(prog.base + src), len);
}
