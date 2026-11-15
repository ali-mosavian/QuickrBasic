/* llrm's QB runtime in the interpreter: what it asks of a compiled program, given here.
 *
 * # How it works
 * The runtime is built from llrm's tree (Makefile, QBRT_*). It asks for the compiled module's data
 * (module.h): this program has one, empty, whose ON ERROR handler is always there, so every error
 * the runtime raises comes to qbrt_land rather than ending the process. qbrt_land raises it as
 * QBasic's error through the session's fault, which unwinds out of the runtime.
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | qbrt_init, module_*, md_*, qbrt_land | NEW |
 */
#include "qbrt/qbrt.h"
#include "error.h"
#include "module.h"
#include "rtinit.h"

struct ModuleData {
    char unused;
};

static ModuleData module;
static Fault *fault;

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

void qbrt_init(Fault *const f)
{
    static bool started;
    fault = f;

    if (started)
        return;

    started = true;
    qb_lander = qbrt_land;
    qbrt_device();
    qb_start();
}
