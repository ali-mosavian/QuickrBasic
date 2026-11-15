/* llrm's QB runtime in the interpreter: what it asks of a compiled program, given here.
 *
 * # How it works
 * The runtime is built from llrm's tree (Makefile, QBRT_*). It asks for the compiled module's data
 * (module.h): this program has one, empty, whose ON ERROR handler is always there, so every error
 * the runtime raises comes to qbrt_land rather than ending the process. qbrt_land raises it as
 * QBasic's error through the session's fault, which unwinds out of the runtime. A wait for the
 * music (play.c's QB_IDLE, the Makefile's) is qbrt_idle: BREAK empties the queue so it ends.
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | qbrt_init, module_*, md_*, qbrt_land, qbrt_palette_using, qbrt_get, qbrt_put, qbrt_screen, qbrt_idle | NEW |
 */
#include "qbrt/qbrt.h"
#include "ad.h"
#include "dev.h"
#include "error.h"
#include "module.h"
#include "rtinit.h"

struct ModuleData {
    char unused;
};

static ModuleData module;
static Fault *fault;
static volatile uint8_t *stop;

extern void (*qb_lander)(void); /* error.c's: ON ERROR's landing */

int (*cn_key_hook)(void); /* KEY's trapping in the runtime's waits: the interpreter traps its own */

ModuleData *module_data(void) { return &module; }

void module_name(char *const name)
{
    for (int i = 0; i < MODULE_NAME_LENGTH; i++)
        name[i] = "QUICKR  "[i];
}

unsigned module_code_segment(void) { return 0; }

unsigned md_on_error(const ModuleData *const m) { return 1; } /* qbrt_land, always */

void md_set_on_error(ModuleData *const m, const unsigned offset) {}

const char *module_first_data(void) { return nullptr; }

int md_restored(const ModuleData *const m) { return 0; }

void md_set_restored(ModuleData *const m) {}

const char *md_cursor(const ModuleData *const m) { return nullptr; }

void md_set_cursor(ModuleData *const m, const char *const cursor) {}

/* The runtime's error b_errnum: raised as QBasic's, out of the runtime. */
[[noreturn]]
static void qbrt_land(void)
{
    b_inonerr = 0;
    fault_qberr(fault, (uint16_t)b_errnum);
}

void B_PALU(qb_data_ptr data, const AD *ad);
void B_GGET(qb_data_ptr data, const AD *ad);
void B_GPUT(qb_data_ptr data, const AD *ad, short how);

/* The runtime's descriptor of bytes elements of elem bytes at data: one dimension. */
static AD qbrt_ad(void *const data, const uint32_t bytes, const uint32_t elem)
{
    return (AD){.data = data, .size = bytes, .dims = 1, .elem = elem, .dm = {{bytes / elem, 0}}};
}

void qbrt_palette_using(void *const data, const uint32_t bytes, const uint32_t elem)
{
    const AD ad = qbrt_ad(data, bytes, elem);
    B_PALU(data, &ad);
}

void qbrt_get(void *const data, const uint32_t bytes, const uint32_t elem)
{
    const AD ad = qbrt_ad(data, bytes, elem);
    B_GGET(data, &ad);
}

void qbrt_put(void *const data, const uint32_t bytes, const uint32_t elem, const short how)
{
    const AD ad = qbrt_ad(data, bytes, elem);
    B_GPUT(data, &ad, how);
}

bool qbrt_screen(void) { return dev_stdout_is_screen(); }

void qbrt_idle(void)
{
    if (*stop & SF_BRK)
        music_clear();
    else
        qbrt_tick_wait();
}

void qbrt_init(Fault *const f, volatile uint8_t *const brk)
{
    static bool started;
    fault = f;
    stop = brk;

    if (started)
        return;

    started = true;
    qb_lander = qbrt_land;
    qbrt_device();
    qb_start();
}
