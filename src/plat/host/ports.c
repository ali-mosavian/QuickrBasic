/* INP and OUT on a host: the VGA's DAC (the runtime's, the colours shown) and status, the 8042's command to type a
   key. */
#include <stdatomic.h>

#include "plat/host/host.h"
#include "plat/psys.h"
#include "qbrt/qbrt.h"

enum : uint16_t {
    PORT_KBD_DATA = 0x60,
    PORT_KBD_CMD = 0x64,
    PORT_DAC_READ = 0x3C7,
    PORT_DAC_WRITE = 0x3C8,
    PORT_DAC_DATA = 0x3C9,
    PORT_STATUS = 0x3DA,
};
enum : uint8_t { KBD_WRITE_OUTPUT = 0xD2, RETRACE = 8, NO_KEYBOARD = 0xFF };

static struct {
    uint8_t status;
    bool kbd_write; /* the next data byte is a key typed */
    uint8_t scan;   /* the keyboard's last byte read */
    uint32_t seen;  /* the frames shown when the retrace was last read as begun */
} ports = {.scan = NO_KEYBOARD};

/* The keyboard's bytes not yet read, pushed by a window's thread and read by the program's. */
static HostScans scans;

HostScans *host_scans(void) { return &scans; }

void host_scan_push(const uint8_t scan)
{
    const uint32_t at = atomic_load(&scans.pushed);

    if (at - atomic_load(&scans.read) == HOST_SCANS)
        return;

    scans.bytes[at % HOST_SCANS] = scan;
    atomic_store(&scans.pushed, at + 1);
}

/* The frames a display has shown. */
static _Atomic uint32_t frames;

_Atomic uint32_t *host_frames(void) { return &frames; }

void host_frame_shown(void) { atomic_fetch_add(&frames, 1); }

uint8_t psys_inp(const uint16_t port)
{
    switch (port) {
    case PORT_DAC_READ:
    case PORT_DAC_WRITE:
    case PORT_DAC_DATA:
        return (uint8_t)qb_port_in(port);
    case PORT_KBD_DATA: {
        /* a byte a look, in order: a press and a release between two looks are both seen */
        const uint32_t at = atomic_load(&scans.read);

        if (at != atomic_load(&scans.pushed)) {
            ports.scan = scans.bytes[at % HOST_SCANS];
            atomic_store(&scans.read, at + 1);
        }

        return ports.scan;
    }

    case PORT_STATUS: {
        const uint32_t shown = atomic_load(&frames);

        if (!shown) /* no display: retrace starts and ends at each look, so a wait for either ends */
            return ports.status ^= RETRACE;

        /* a display's: begun once for each frame shown, over at the next look. A program that waits for it steps
           with the display; one too slow for a frame sees the frames it missed as one */
        if (shown == ports.seen)
            return 0;

        ports.seen = shown;
        return RETRACE;
    }
    default:
        return 0xFF;
    }
}

void psys_outp(const uint16_t port, const uint8_t v)
{
    switch (port) {
    case PORT_DAC_READ:
    case PORT_DAC_WRITE:
    case PORT_DAC_DATA:
        qb_port_out(port, v);
        break;

    case PORT_KBD_CMD:
        ports.kbd_write = v == KBD_WRITE_OUTPUT;
        break;

    case PORT_KBD_DATA:
        if (ports.kbd_write)
            host_key_push((uint16_t)(v << 8));

        ports.kbd_write = false;
        break;

    default:
        break;
    }
}
