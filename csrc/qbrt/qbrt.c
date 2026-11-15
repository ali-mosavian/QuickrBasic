/* llrm's QB runtime in the interpreter: what it asks of a compiled program, given here.
 *
 * # How it works
 * The runtime is built from llrm's tree (Makefile, QBRT_*). It asks for the compiled module's data
 * (module.h): this program has one, empty, whose ON ERROR handler is always there, so every error
 * the runtime raises comes to qbrt_land rather than ending the process. qbrt_land raises it as
 * QBasic's error through the session's fault, which unwinds out of the runtime.
 *
 * Its memory is the arena's: its strings and local heap a region boot carves (they hold still); a
 * PAINT's queue the BASIC heap's free part, lent for the fill (nothing else allocates during one).
 * Its initializers are called here, in its modules' order (xi.h only declares them). A wait for the
 * music (play.c's QB_IDLE, the Makefile's) is qbrt_idle: BREAK empties the queue so it ends.
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | qbrt_init, module_*, md_*, qbrt_land, qbrt_palette_using, qbrt_get, qbrt_put, qbrt_screen, qbrt_idle, qbrt_done, qbrt_varptr | NEW |
 */
#include "qbrt/qbrt.h"
#include "ad.h"
#include "cndriver.h"
#include "dev.h"
#include "error.h"
#include "gfx.h"
#include "module.h"
#include "rtinit.h"
#include "varptr.h"
#include "base.h"

struct ModuleData {
    char unused;
};

static ModuleData module;
static Fault *fault;
static volatile uint8_t *stop;
static char *heap, *heap_top;
static void *lent;
static uint32_t lent_bytes;

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

/* DOS's answer, asked once: an IOCTL is a mode switch, and PRINT asks for each character. */
bool qbrt_printer_ready(void) { return dev_printer_ready(); }

bool qbrt_screen(void)
{
    static int8_t screen = -1;

    if (screen < 0)
        screen = (int8_t)dev_stdout_is_screen();

    return screen;
}

void qbrt_console_reset(void)
{
    cn_mode_changed();
    cn_cls();
}

void qbrt_where(uint8_t *const row, uint8_t *const col)
{
    *row = cn_line();
    *col = cn_pos();
}

void qbrt_move(const uint8_t row, const uint8_t col) { cn_locate(row + 1, col + 1, -1); }

bool qbrt_graphics(void) { return gfx_current && gfx_current->number; }

void qbrt_text(void)
{
    if (qbrt_graphics())
        gfx_screen(0);
}

void cn_text_xinit(void);
void cn_gfx_xinit(void);
void nh_xinit(void);
void gfx_xinit(void);
void play_xinit(void);

void qb_run_initializers(void)
{
    nh_xinit();
    cn_text_xinit();
    cn_gfx_xinit();
    gfx_xinit();
    play_xinit();
}

void qb_dynamic_region(char **const first, char **const top)
{
    *first = heap;
    *top = heap_top;
}

void qbrt_lend(void *const room, const uint32_t bytes)
{
    lent = room;
    lent_bytes = bytes;
}

void *qb_paint_queue_open(unsigned long *const bytes)
{
    *bytes = lent_bytes;
    return lent;
}

void qb_paint_queue_close(void) { qbrt_lend(nullptr, 0); }

void qbrt_idle(void)
{
    if (*stop & SF_BRK)
        music_clear();
    else
        qbrt_tick_wait();
}

void qbrt_done(void)
{
    qbrt_text();
    dev_tone(0);
    dev_tick_stop();
}

/* VARPTR$'s address, an Addr: its variable. */
static void *vm_variable(const unsigned address) { return at(address); }

/* A VARPTR$'s string variable, the VM's packed Desc {len, ptr}: as an SD. */
static const SD *vm_string(const void *const desc)
{
    static SD s;
    s.len = load16(desc);
    s.ptr = (char *)at(load32((const uint8_t *)desc + 2));
    return &s;
}

SD *B_VARP(unsigned address, short type); /* varptr.c, nhstutil.c */
void str_tmp_free(SD *sd);

void qbrt_varptr(const uint32_t address, const uint8_t type, char out[static QBRT_VARPTR])
{
    SD *const s = B_VARP(address, type);
    memcpy(out, s->ptr, QBRT_VARPTR);
    str_tmp_free(s);
}

void qbrt_init(Fault *const f, volatile uint8_t *const brk, void *const room, const uint32_t bytes)
{
    static bool started;
    fault = f;
    stop = brk;
    heap = room;
    heap_top = heap + bytes;

    if (started)
        return;

    started = true;
    qb_lander = qbrt_land;
    varptr_at = vm_variable;
    varptr_desc = vm_string;
    qbrt_device();
    qb_start();
}
