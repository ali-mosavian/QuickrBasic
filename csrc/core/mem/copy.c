#include "mem/copy.h"
#include "mem/prog.h"

void copy_mem(const Addr dst, const Addr src, const uint32_t len) { memmove(at(dst), at(src), len); }

Addr copy_zero(const Addr a, const uint32_t len)
{
    memset(at(a), 0, len);
    return a + len;
}

void copy_prog(const uint32_t dst, const uint32_t src, const uint32_t len)
{
    memmove(at(prog.base + dst), at(prog.base + src), len);
}
