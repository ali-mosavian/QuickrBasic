/* PCI's configuration space (pci.h). */
#include "pci.h"
#include "dos.h"

enum : uint16_t { ADDRESS = 0xCF8, DATA = 0xCFC };

enum : uint32_t {
    ENABLE = 0x80000000,
    BUSES = 256,
    DEVICES = 32,
    FUNCTIONS = 8,
    ID = 0x00,
    CLASS = 0x08,
    HEADER = 0x0C,
    MULTIFUNCTION = 0x800000,
    NONE = 0xFFFF,
};

/* The bus address of register reg of the function at bus, device and function. */
static uint32_t address(
    const uint8_t bus,
    const uint8_t device,
    const uint8_t function,
    const uint8_t reg
)
{
    return ENABLE | (uint32_t)bus << 16 | (uint32_t)device << 11 | (uint32_t)function << 8 |
           (reg & 0xFCu);
}

static uint32_t read_at(const uint32_t at)
{
    pdos_outd(ADDRESS, at);
    return pdos_ind(DATA);
}

uint32_t pci_read(const PciFunction *const f, const uint8_t reg)
{
    return read_at(address(f->bus, f->device, f->function, reg));
}

void pci_write(const PciFunction *const f, const uint8_t reg, const uint32_t v)
{
    pdos_outd(ADDRESS, address(f->bus, f->device, f->function, reg));
    pdos_outd(DATA, v);
}

/* Mechanism #1 answers: its address port keeps what was written. */
static bool present(void)
{
    const uint32_t was = pdos_ind(ADDRESS);

    pdos_outd(ADDRESS, ENABLE);
    const bool kept = pdos_ind(ADDRESS) == ENABLE;
    pdos_outd(ADDRESS, was);
    return kept;
}

/* The function's, kept in out when a display's or a multimedia's and room is left. */
static uint32_t found(
    const uint8_t bus,
    const uint8_t device,
    const uint8_t function,
    const uint32_t id,
    PciFunction *const out,
    const uint32_t n,
    const uint32_t most
)
{
    const uint32_t class_code = read_at(address(bus, device, function, CLASS)) >> 8;
    const uint8_t base = (uint8_t)(class_code >> 16);

    if (n >= most || (base != PCI_DISPLAY && base != PCI_MULTIMEDIA))
        return n;

    out[n] = (PciFunction){bus, device, function, (uint16_t)id, (uint16_t)(id >> 16), class_code};

    return n + 1;
}

uint32_t pci_cards(PciFunction *const out, const uint32_t most)
{
    uint32_t n = 0;

    if (!present())
        return 0;

    for (uint32_t bus = 0; bus < BUSES; bus++)
        for (uint32_t device = 0; device < DEVICES; device++) {
            const uint32_t id = read_at(address((uint8_t)bus, (uint8_t)device, 0, ID));

            if ((id & NONE) == NONE)
                continue;

            const bool several =
                read_at(address((uint8_t)bus, (uint8_t)device, 0, HEADER)) & MULTIFUNCTION;

            n = found((uint8_t)bus, (uint8_t)device, 0, id, out, n, most);

            for (uint32_t function = 1; several && function < FUNCTIONS; function++) {
                const uint32_t more =
                    read_at(address((uint8_t)bus, (uint8_t)device, (uint8_t)function, ID));

                if ((more & NONE) != NONE)
                    n = found((uint8_t)bus, (uint8_t)device, (uint8_t)function, more, out, n, most);
            }
        }

    return n;
}
