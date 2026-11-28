/* PCI's configuration space (pci.h). */
#include "pci.h"

enum : uint16_t {
    ADDRESS = 0xCF8, /* #1's address; #2's enable and function */
    PICKED = 0xCFB,  /* on chipsets with both: 1 picks #1, 0 #2 */
    FORWARD = 0xCFA, /* #2's bus */
    DATA = 0xCFC,    /* #1's */
    SPACE = 0xC000,  /* #2's: device d's registers from SPACE | d << 8 */
};

enum : uint32_t {
    ENABLE = 0x80000000,
    KEY = 0xF0, /* #2's enable, its function from the 2nd bit */
    BUSES = 256,
    FUNCTIONS = 8,
    ID = 0x00,
    CLASS = 0x08,
    HEADER = 0x0C,
    MULTIFUNCTION = 0x800000,
    NONE = 0xFFFF,
};

/* A function's place, as a mechanism reaches it. */
typedef struct {
    uint8_t bus, device, function;
} Place;

/* A mechanism's: a register read and written, and the devices a bus has. */
typedef struct {
    uint32_t (*read)(const PciPorts *p, Place at, uint8_t reg);
    void (*write)(const PciPorts *p, Place at, uint8_t reg, uint32_t v);
    uint32_t devices;
} Mechanism;

static uint32_t address(const Place at, const uint8_t reg)
{
    return ENABLE | (uint32_t)at.bus << 16 | (uint32_t)at.device << 11 |
           (uint32_t)at.function << 8 | (reg & 0xFCu);
}

static uint32_t read1(const PciPorts *const p, const Place at, const uint8_t reg)
{
    p->outd(ADDRESS, address(at, reg));
    return p->ind(DATA);
}

static void write1(const PciPorts *const p, const Place at, const uint8_t reg, const uint32_t v)
{
    p->outd(ADDRESS, address(at, reg));
    p->outd(DATA, v);
}

/* #2's space opened on at's bus and function: device d's register reg's port. */
static uint16_t opened(const PciPorts *const p, const Place at, const uint8_t reg)
{
    p->outb(ADDRESS, (uint8_t)(KEY | at.function << 1));
    p->outb(FORWARD, at.bus);
    return (uint16_t)(SPACE | at.device << 8 | (reg & 0xFCu));
}

static uint32_t read2(const PciPorts *const p, const Place at, const uint8_t reg)
{
    const uint32_t v = p->ind(opened(p, at, reg));

    p->outb(ADDRESS, 0);
    return v;
}

static void write2(const PciPorts *const p, const Place at, const uint8_t reg, const uint32_t v)
{
    p->outd(opened(p, at, reg), v);
    p->outb(ADDRESS, 0);
}

static const Mechanism MECHANISMS[] = {{}, {read1, write1, 32}, {read2, write2, 16}};

/* #1 answers: its address port keeps what was written. */
static bool answers1(const PciPorts *const p)
{
    p->outb(PICKED, 1);
    const uint32_t was = p->ind(ADDRESS);

    p->outd(ADDRESS, ENABLE);
    const bool kept = p->ind(ADDRESS) == ENABLE;
    p->outd(ADDRESS, was);
    return kept;
}

/* #2 answers: its enable and bus ports keep a 0. */
static bool answers2(const PciPorts *const p)
{
    p->outb(PICKED, 0);
    p->outb(ADDRESS, 0);
    p->outb(FORWARD, 0);
    return p->inb(ADDRESS) == 0 && p->inb(FORWARD) == 0;
}

PciBus pci_bus(const PciPorts *const ports)
{
    return (PciBus){ports, answers1(ports) ? 1 : answers2(ports) ? 2 : 0};
}

static uint32_t read_at(const PciBus *const b, const Place at, const uint8_t reg)
{
    return MECHANISMS[b->mechanism].read(b->ports, at, reg);
}

uint32_t pci_read(const PciBus *const b, const PciFunction *const f, const uint8_t reg)
{
    return read_at(b, (Place){f->bus, f->device, f->function}, reg);
}

void pci_write(
    const PciBus *const b,
    const PciFunction *const f,
    const uint8_t reg,
    const uint32_t v
)
{
    MECHANISMS[b->mechanism].write(b->ports, (Place){f->bus, f->device, f->function}, reg, v);
}

/* The function's, kept in out when a display's or a multimedia's and room is left. */
static uint32_t found(
    const PciBus *const b,
    const Place at,
    const uint32_t id,
    PciFunction *const out,
    const uint32_t n,
    const uint32_t most
)
{
    const uint32_t class_code = read_at(b, at, CLASS) >> 8;
    const uint8_t base = (uint8_t)(class_code >> 16);

    if (n >= most || (base != PCI_DISPLAY && base != PCI_MULTIMEDIA))
        return n;

    out[n] = (PciFunction){
        at.bus,
        at.device,
        at.function,
        (uint16_t)id,
        (uint16_t)(id >> 16),
        class_code,
    };

    return n + 1;
}

uint32_t pci_cards(const PciBus *const b, PciFunction *const out, const uint32_t most)
{
    uint32_t n = 0;

    if (!b->mechanism)
        return 0;

    for (uint32_t bus = 0; bus < BUSES; bus++)
        for (uint32_t device = 0; device < MECHANISMS[b->mechanism].devices; device++) {
            const Place first = {(uint8_t)bus, (uint8_t)device, 0};
            const uint32_t id = read_at(b, first, ID);

            if ((id & NONE) == NONE)
                continue;

            const bool several = read_at(b, first, HEADER) & MULTIFUNCTION;

            n = found(b, first, id, out, n, most);

            for (uint32_t function = 1; several && function < FUNCTIONS; function++) {
                const Place at = {(uint8_t)bus, (uint8_t)device, (uint8_t)function};
                const uint32_t more = read_at(b, at, ID);

                if ((more & NONE) != NONE)
                    n = found(b, at, more, out, n, most);
            }
        }

    return n;
}
