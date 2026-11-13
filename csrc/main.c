#include "base.h"
#include "plat/pmem.h"
#include <llrm_os.h>

static void say(const char *s) { llrm_os_write_file(1, (const unsigned char *)s, strlen(s)); }

static void num(uint32_t v)
{
    char b[12];
    int n = 0;
    do b[n++] = '0' + v % 10; while (v /= 10);
    while (n) llrm_os_write_file(1, (const unsigned char *)&b[--n], 1);
}

int main(void)
{
    Addr lo, hi;
    if (!pmem_init(0x40000, &lo, &hi)) {
        say("Out of memory\r\n");
        return 1;
    }
    say("arena ");
    num(lo);
    say(" ");
    num(hi - lo);
    wr32(lo, 0x12345678);
    say(rd32(lo) == 0x12345678 ? " ok\r\n" : " bad\r\n");
    return 0;
}
