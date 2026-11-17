/* INP and OUT on a host: the VGA's DAC and status, the 8042's command to type a key. */
#include "plat/host/host.h"
#include "plat/psys.h"

enum : uint16_t {
    PORT_KBD_DATA = 0x60,
    PORT_KBD_CMD = 0x64,
    PORT_DAC_READ = 0x3C7,
    PORT_DAC_WRITE = 0x3C8,
    PORT_DAC_DATA = 0x3C9,
    PORT_STATUS = 0x3DA,
};
enum : uint8_t { KBD_WRITE_OUTPUT = 0xD2, RETRACE = 8 };

static struct {
    uint8_t dac[256][3];
    uint8_t index, part;
    uint8_t status;
    bool kbd_write; /* the next data byte is a key typed */
} ports;

uint8_t psys_inp(const uint16_t port)
{
    switch (port) {
    case PORT_DAC_DATA: {
        const uint8_t v = ports.dac[ports.index][ports.part];

        if (++ports.part == 3) {
            ports.part = 0;
            ports.index++;
        }

        return v;
    }
    case PORT_STATUS:
        return ports.status ^= RETRACE; /* retrace starts and ends at each look */
    default:
        return 0xFF;
    }
}

void psys_outp(const uint16_t port, const uint8_t v)
{
    switch (port) {
    case PORT_DAC_READ:
    case PORT_DAC_WRITE:
        ports.index = v;
        ports.part = 0;
        break;

    case PORT_DAC_DATA:
        ports.dac[ports.index][ports.part] = v & 0x3F;

        if (++ports.part == 3) {
            ports.part = 0;
            ports.index++;
        }

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
