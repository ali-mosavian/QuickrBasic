/* The process on DOS32: the STOP key's vectors (pstop.asm), the PSP's command line and
   environment (llrm's OS layer), SHELL's EXEC through DPMI in DOS memory. */
#include <llrm_os.h>
#include "dos.h"
#include "plat/psys.h"

void pstop_hook(volatile uint8_t *flag, uint8_t bit);
void pstop_unhook(void);
void ptick_unhook(void); /* pclk's and pkey's tick (ptick.asm) */

void psys_init(volatile uint8_t *stop, uint8_t bit) { pstop_hook(stop, bit); }

void psys_exit(uint8_t code)
{
    pstop_unhook();
    ptick_unhook();
    PdosRegs r = {.eax = 0x4C00u | code};
    pdos_int(0x21, &r);
    for (;;) {
    }
}

uint32_t psys_cmdline(char *buf, uint32_t cap)
{
    int n = llrm_os_command_line((unsigned char *)buf, cap - 1);
    if (n < 0)
        n = 0;
    buf[n] = 0;
    return (uint32_t)n;
}

uint32_t psys_environ(char *buf, uint32_t cap)
{
    uint32_t n = llrm_os_environment_read(0, (unsigned char *)buf, cap - 1);
    for (uint32_t i = 0; i < n; i++)
        if (!buf[i] && (!i || !buf[i - 1]))
            return i + 1;
    buf[n] = 0;
    return n + 1;
}

/* The DOS block's layout after the environment: the parameter block, the tail, the path, the
   FCBs. */
enum { XP = 0, XT = 16, XN = XT + 128, XF = XN + 128, XSZ = XF + 32 };

/* DS's base: a linear address less it is a DS offset. */
static uint32_t ds_base(void)
{
    PdosRegs r = {.eax = 0x0006, .ebx = pdos_ds()};
    pdos_int(0x31, &r);
    return (r.ecx & 0xFFFF) << 16 | (r.edx & 0xFFFF);
}

bool psys_exec(const char *path, const char *tail, uint8_t len, const char *env,
               uint32_t envlen)
{
    uint32_t xenv = (envlen + 16) & ~15u; /* the environment and a 0, on a paragraph */
    PdosRegs r = {.eax = 0x0100, .ebx = (xenv + XSZ + 15) >> 4};
    if (pdos_int(0x31, &r) & PDOS_CF)
        return false;
    uint16_t seg = (uint16_t)r.eax, sel = (uint16_t)r.edx;
    uint8_t *b = (uint8_t *)(uintptr_t)((uint32_t)seg * 16 - ds_base());
    memset(b, 0, xenv + XSZ);
    memcpy(b, env, envlen);
    uint16_t pb[7] = {seg, (uint16_t)(xenv + XT), seg, (uint16_t)(xenv + XF), seg,
                      (uint16_t)(xenv + XF + 16), seg};
    memcpy(b + xenv + XP, pb, sizeof pb);
    uint8_t *t = b + xenv + XT; /* its count, its text, CR */
    t[0] = len;
    memcpy(t + 1, tail, len);
    t[1 + len] = 13;
    char *n = (char *)b + xenv + XN;
    for (int i = 0; i < 127 && (n[i] = path[i]); i++) {
    }

    static uint8_t rm[0x32]; /* the real-mode registers */
    memset(rm, 0, sizeof rm);
    uint32_t v = 0x4B00, bx = xenv + XP, dx = xenv + XN;
    memcpy(rm + 0x1C, &v, 4);  /* EAX */
    memcpy(rm + 0x10, &bx, 4); /* EBX: the parameter block */
    memcpy(rm + 0x14, &dx, 4); /* EDX: the path */
    memcpy(rm + 0x22, &seg, 2); /* ES */
    memcpy(rm + 0x24, &seg, 2); /* DS */

    PdosRegs v22 = {.eax = 0x0200, .ebx = 0x22}; /* EXEC leaves INT 22h its own: kept */
    pdos_int(0x31, &v22);
    r = (PdosRegs){.eax = 0x0300, .ebx = 0x21, .ecx = 0, .edi = (uint32_t)(uintptr_t)rm};
    bool ran = !(pdos_int(0x31, &r) & PDOS_CF) && !(rm[0x20] & 1);
    v22.eax = 0x0201;
    v22.ebx = 0x22;
    pdos_int(0x31, &v22);
    r = (PdosRegs){.eax = 0x0101, .edx = sel};
    pdos_int(0x31, &r);
    return ran;
}
