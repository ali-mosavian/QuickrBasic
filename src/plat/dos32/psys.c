/* The process on DOS32.
 *
 * # How it works
 * The STOP key's vectors (pstop.asm), the PSP's command line and environment (llrm's OS layer),
 * SHELL's EXEC through DPMI in DOS memory.
 */
#include "dos.h"
#include "plat/psys.h"
#include "plat/pmem.h"

enum { ENV_MAX = 0x10000 }; /* the environment and the program's path at most */

CDECL void pstop_hook(volatile uint8_t *flag, uint8_t bit);
CDECL void pstop_unhook(void);
CDECL void ptick_unhook(void); /* pclk's and pkey's tick (ptick.asm) */

enum { INT_TERMINATE = 0x22, CH_CR = 0x0D, PATH_MAX = 127 };

void psys_init(volatile uint8_t *const stop, const uint8_t bit) { pstop_hook(stop, bit); }

void psys_exit(const uint8_t code)
{
    pstop_unhook();
    ptick_unhook();
    PdosRegs r = {.eax = pdos_fn(DOS_EXIT, code)};

    pdos_int(INT_DOS, &r);

    for (;;) {
    }
}

uint32_t psys_cmdline(char *const buf, const uint32_t cap)
{
    const uint32_t n = pdos_cmdline(buf, cap - 1);
    buf[n] = 0;
    return n;
}

uint32_t psys_environ(char *const buf, const uint32_t cap)
{
    const uint32_t n = pdos_environ(0, (uint8_t *)buf, cap - 1);

    for (uint32_t i = 0; i < n; i++)
        if (!buf[i] && (!i || !buf[i - 1]))
            return i + 1;

    buf[n] = 0;
    return n + 1;
}

uint32_t psys_self(char *const buf, const uint32_t cap)
{
    char *env = nullptr;
    uint32_t n = 0;

    for (uint32_t size = 0x400; size <= ENV_MAX; size *= 2) { /* till the block fits */
        char *const grown = pmem_resize(env, size);

        if (!grown)
            break;

        env = grown;
        n = pdos_environ(0, (uint8_t *)env, size);

        if (n < size)
            break;
    }

    uint32_t i = 0;

    while (i + 1 < n && (env[i] || env[i + 1])) /* the variables, a 0 after the last */
        i++;

    i += 4; /* the two 0s, the strings' count */
    uint32_t len = 0;

    for (; i < n && env[i] && len + 1 < cap; i++)
        buf[len++] = env[i];

    buf[len] = 0;
    pmem_free(env);
    return len;
}

/* EXEC's parameter block (4B00h): far pointers as offset, segment. */
typedef struct __attribute__((packed)) {
    uint16_t env_seg, tail_off, tail_seg, fcb1_off, fcb1_seg, fcb2_off, fcb2_seg;
} ExecParams;

/* The DOS block after the environment: the parameter block, the tail, the path, two FCBs. */
typedef struct __attribute__((packed)) {
    ExecParams params;
    uint8_t pad[16 - sizeof(ExecParams)];
    uint8_t tail[128]; /* its count, its text, CR */
    char path[128];
    uint8_t fcb[2][16];
} ExecBlock;

/* The environment at the block's start, the EXEC block at xenv. */
static void fill(
    const PdosBlock *const b,
    const uint32_t xenv,
    const char *const path,
    const char *const tail,
    const uint8_t len,
    const char *const env,
    const uint32_t envlen
)
{
    memset(b->at, 0, xenv + sizeof(ExecBlock));

    for (uint32_t i = 0; i < envlen; i++)
        b->at[i] = (uint8_t)env[i];

    ExecBlock *const x = (ExecBlock *)(b->at + xenv);
    const uint16_t base = (uint16_t)xenv;
    x->params = (ExecParams){
        .env_seg = b->seg,
        .tail_off = base + offsetof(ExecBlock, tail),
        .tail_seg = b->seg,
        .fcb1_off = base + offsetof(ExecBlock, fcb[0]),
        .fcb1_seg = b->seg,
        .fcb2_off = base + offsetof(ExecBlock, fcb[1]),
        .fcb2_seg = b->seg,
    };

    x->tail[0] = len;

    for (uint32_t i = 0; i < len; i++)
        x->tail[1 + i] = (uint8_t)tail[i];

    x->tail[1 + len] = CH_CR;

    for (uint32_t i = 0; i < PATH_MAX && (x->path[i] = path[i]); i++) {
    }
}

/* DOS's EXEC of the block at xenv, INT 22h kept (EXEC takes it): whether it ran. */
static bool exec(const PdosBlock *const b, const uint32_t xenv)
{
    static PdosRmRegs rm;
    rm = (PdosRmRegs){
        .eax = pdos_fn(DOS_EXEC, 0),
        .ebx = xenv + offsetof(ExecBlock, params),
        .edx = xenv + offsetof(ExecBlock, path),
        .es = b->seg,
        .ds = b->seg,
    };

    PdosRegs v22 = {.eax = DPMI_GET_RM_VECTOR, .ebx = INT_TERMINATE};

    pdos_int(INT_DPMI, &v22);
    const bool ran = pdos_rm_int(INT_DOS, &rm) && !(rm.flags & PDOS_CF);
    v22.eax = DPMI_SET_RM_VECTOR;
    v22.ebx = INT_TERMINATE;
    pdos_int(INT_DPMI, &v22);
    return ran;
}

bool psys_exec(
    const char *const path,
    const char *const tail,
    const uint8_t len,
    const char *const env,
    const uint32_t envlen
)
{
    const uint32_t xenv = (envlen + 16) & ~15u; /* the environment and a 0, on a paragraph */
    PdosBlock b;

    if (!pdos_alloc(xenv + sizeof(ExecBlock), &b))
        return false;

    fill(&b, xenv, path, tail, len, env, envlen);
    const bool ran = exec(&b, xenv);
    pdos_free(&b);
    return ran;
}
