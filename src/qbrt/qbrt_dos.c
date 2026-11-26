/* The runtime's device on DOS32: llrm's own (dev/dos, its asm), and what its OS layer and start-up
 * give it, from this platform's.
 *
 * # How it works
 * The tick is the timer interrupt's (tick.asm), so a wait for the music only waits.
 *
 * A shot of the screen is the visible page as the mode lays it out: 13h's 64000 bytes, the CGA
 * modes' 16K at B800h, else each plane's page (read map select), with the DAC, the attribute
 * palette and the CRTC's display start. It is shown again through the BIOS's mode set with bit 7
 * (memory kept), each plane written in write mode 0, then the controller left as the runtime
 * keeps it: write mode 2, function 0, bit mask FFh, all planes.
 */
#include "qbrt/qbrt.h"
#include "device.h"
#include "gfx.h"
#include "llrm_os.h"
#include "plat/pfil.h"
#include "plat/pkey.h"
#include "plat/psys.h"

long llrm_os_write_file(const short handle, const unsigned char *const p, const unsigned n)
{
    uint32_t put;
    return pfil_write((PfilHandle)handle, p, n, &put) ? -1 : (long)put;
}

void llrm_os_exit(const unsigned char code) { psys_exit(code); }

unsigned char llrm_os_console_key_ready(void) { return pkey_ready(); }

enum { ENV_MOST = 0x8000 }; /* DOS's */

/* The environment's string `index` (NAME=value) in data, max bytes at most; its length, -1 past the last. Its copy
   is taken for the call. */
int llrm_os_environment(const unsigned index, unsigned char *const data, const unsigned max)
{
    char *const env = dev_take(ENV_MOST);
    const uint32_t n = psys_environ(env, ENV_MOST);
    uint32_t at = 0;
    int found = -1;

    for (unsigned i = 0; at < n && env[at]; i++) {
        uint32_t length = 0;

        while (at + length < n && env[at + length])
            length++;
        if (i == index) {
            const uint32_t put = length < max ? length : max;
            for (uint32_t k = 0; k < put; k++)
                data[k] = (unsigned char)env[at + k];

            found = (int)put;
            break;
        }
        at += length + 1;
    }

    dev_give(env);
    return found;
}

unsigned char llrm_os_console_read_key(void) { return (unsigned char)pkey_wait(); }

void qb_sincos(const double *const angle, double *const sine, double *const cosine)
{
    __asm__("fsincos" : "=t"(*cosine), "=u"(*sine) : "0"(*angle));
}

void qbrt_device(void) {}

void qbrt_realtime(const bool yes) {}

void qbrt_tick_wait(void) {}

void qbrt_tick_poll(void) {}

void qbrt_clock_set(const PclkTime *const t) {}

enum {
    MODE_LINEAR = 0x13,
    MODE_CGA4 = 4,
    MODE_CGA2 = 6,
    KEEP_MEMORY = 0x80,
    CGA_BYTES = 0x4000,
    PLANES = 4,
    PLANE_MOST = 640 / 8 * 480,
    SEQ = 0x3C4,
    SEQ_MAP_MASK = 2,
    GC = 0x3CE,
    GC_SET_RESET_ENABLE = 1,
    GC_FUNCTION = 3,
    GC_READ_MAP = 4,
    GC_MODE = 5,
    GC_BIT_MASK = 8,
    GC_WRITE_MODE_2 = 2,
    CRTC = 0x3D4,
    CRTC_START_HIGH = 0x0C,
    CRTC_START_LOW = 0x0D,
    DAC_READ = 0x3C7,
    DAC_WRITE = 0x3C8,
    DAC_DATA = 0x3C9,
    DAC_BYTES = 256 * 3,
    ATTRS = 16,
    BIOS_SET_ATTR = 0x1000,
    BIOS_GET_ATTR = 0x1007,
};

typedef struct {
    uint16_t start;         /* the CRTC's display start: the visible page */
    uint8_t dac[DAC_BYTES]; /* the colour registers */
    uint8_t attr[ATTRS];    /* the attribute palette */
    uint8_t pixels[];       /* the page, plane by plane */
} Shot;


static void reg(const unsigned port, const unsigned index, const unsigned value)
{
    dev_outb(port, index);
    dev_outb(port + 1, value);
}

static unsigned reg_in(const unsigned port, const unsigned index)
{
    dev_outb(port, index);
    return dev_inb(port + 1);
}

typedef enum : uint8_t { LAYOUT_LINEAR, LAYOUT_CGA, LAYOUT_PLANAR } Layout;

static Layout layout(void)
{
    const unsigned bios = gfx_current->bios;

    if (bios == MODE_LINEAR)
        return LAYOUT_LINEAR;

    return bios == MODE_CGA4 || bios == MODE_CGA2 ? LAYOUT_CGA : LAYOUT_PLANAR;
}

/* The visible page's bytes, in one plane. */
static uint32_t page_bytes(const Layout l)
{
    if (l == LAYOUT_LINEAR)
        return gfx_current->width * gfx_current->height;

    return l == LAYOUT_CGA ? CGA_BYTES : gfx_current->width / 8 * gfx_current->height;
}

uint32_t qbrt_shot_size(void)
{
    const Layout l = layout();
    return sizeof(Shot) + (l == LAYOUT_PLANAR ? PLANES : 1u) * page_bytes(l);
}

static uint8_t *video(const Layout l, const uint16_t start)
{
    return (uint8_t *)(l == LAYOUT_CGA ? 0xB8000 : 0xA0000) + start;
}

void qbrt_shot_take(void *const p)
{
    Shot *const shot = p;
    const Layout l = layout();
    shot->start =
        l == LAYOUT_PLANAR
            ? (uint16_t)(reg_in(CRTC, CRTC_START_HIGH) << 8 | reg_in(CRTC, CRTC_START_LOW))
            : 0;
    dev_outb(DAC_READ, 0);

    for (uint32_t i = 0; i < DAC_BYTES; i++)
        shot->dac[i] = (uint8_t)dev_inb(DAC_DATA);

    for (unsigned i = 0; i < ATTRS; i++) {
        Regs r = {.rax = BIOS_GET_ATTR, .rbx = i};

        dev_int10(&r);
        shot->attr[i] = (uint8_t)(r.rbx >> 8);
    }

    const uint32_t n = page_bytes(l);
    const uint8_t *const from = video(l, shot->start);

    if (l != LAYOUT_PLANAR) {
        memcpy(shot->pixels, from, n);
        return;
    }

    for (unsigned plane = 0; plane < PLANES; plane++) {
        reg(GC, GC_READ_MAP, plane);
        memcpy(shot->pixels + plane * n, from, n);
    }

    reg(GC, GC_READ_MAP, 0);
}

void qbrt_shot_drop(void) {}

void qbrt_shot_show(const void *const p)
{
    const Shot *const shot = p;
    const Layout l = layout();
    Regs r = {.rax = gfx_current->bios | KEEP_MEMORY};

    dev_int10(&r);

    for (unsigned i = 0; i < ATTRS; i++) {
        Regs a = {.rax = BIOS_SET_ATTR, .rbx = (unsigned)shot->attr[i] << 8 | i};

        dev_int10(&a);
    }

    dev_outb(DAC_WRITE, 0);

    for (uint32_t i = 0; i < DAC_BYTES; i++)
        dev_outb(DAC_DATA, shot->dac[i]);

    const uint32_t n = page_bytes(l);
    uint8_t *const to = video(l, shot->start);

    if (l != LAYOUT_PLANAR) {
        memcpy(to, shot->pixels, n);
        return;
    }

    reg(GC, GC_MODE, 0);
    reg(GC, GC_FUNCTION, 0);
    reg(GC, GC_BIT_MASK, 0xFF);
    reg(GC, GC_SET_RESET_ENABLE, 0);

    for (unsigned plane = 0; plane < PLANES; plane++) {
        reg(SEQ, SEQ_MAP_MASK, 1u << plane);
        memcpy(to, shot->pixels + plane * n, n);
    }

    reg(SEQ, SEQ_MAP_MASK, 0x0F);
    reg(GC, GC_MODE, GC_WRITE_MODE_2);
    reg(CRTC, CRTC_START_HIGH, shot->start >> 8);
    reg(CRTC, CRTC_START_LOW, shot->start & 0xFF);
}
