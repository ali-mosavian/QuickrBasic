/* plat/dos32/pci.c: the bus found and searched through a fake bridge of each mechanism. */
#include "plat/dos32/pci.h"
#include "unit.h"

enum : uint32_t {
    VOODOO = 0x0001121A, /* 3dfx's Voodoo Graphics, at device 5 of bus 0 */
    CARD = 5,
    BRIDGE = 0x04838086, /* Intel's 82424, at device 0 */
    VIDEO = 0x04000000,
    HOST = 0x06000000,
    BAR0 = 0x10,
    ABSENT = 0xFFFFFFFF,
};

/* A host bridge answering by its mechanism (0, none): #1's address, #2's enable and bus; the card's
   BAR0. */
static struct {
    uint8_t mechanism;
    uint32_t address;
    uint8_t enable, forward;
    uint32_t bar;
} bridge;

static uint32_t *config(
    const uint32_t bus,
    const uint32_t device,
    const uint32_t function,
    const uint32_t reg
)
{
    static uint32_t word;

    word = ABSENT;

    if (bus || function)
        return &word;

    if (device == CARD)
        word = reg == 0 ? VOODOO : reg == 8 ? VIDEO | 2 : 0;
    else if (device == 0)
        word = reg == 0 ? BRIDGE : reg == 8 ? HOST : 0;

    return reg == BAR0 && device == CARD ? &bridge.bar : &word;
}

/* The register a port reaches: null, none. */
static uint32_t *reached(const uint16_t port)
{
    const uint32_t a = bridge.address;

    if (bridge.mechanism == 1 && port == 0xCFC && a & 1u << 31)
        return config(a >> 16 & 0xFF, a >> 11 & 0x1F, a >> 8 & 7, a & 0xFC);

    if (bridge.mechanism == 2 && port >= 0xC000 && port <= 0xCFFF && bridge.enable & 0xF0)
        return config(bridge.forward, port >> 8 & 0xF, bridge.enable >> 1 & 7, port & 0xFC);

    return nullptr;
}

static uint8_t inb(const uint16_t port)
{
    if (bridge.mechanism == 2 && port == 0xCF8)
        return bridge.enable;

    return bridge.mechanism == 2 && port == 0xCFA ? bridge.forward : 0xFF;
}

static void outb(const uint16_t port, const uint8_t v)
{
    if (bridge.mechanism == 2 && port == 0xCF8)
        bridge.enable = v;
    else if (bridge.mechanism == 2 && port == 0xCFA)
        bridge.forward = v;
}

static uint32_t ind(const uint16_t port)
{
    const uint32_t *const r = reached(port);

    if (port == 0xCF8)
        return bridge.mechanism == 1   ? bridge.address
               : bridge.mechanism == 2 ? bridge.enable | (uint32_t)bridge.forward << 16
                                       : ABSENT;

    return r ? *r : ABSENT;
}

static void outd(const uint16_t port, const uint32_t v)
{
    uint32_t *const r = reached(port);

    if (port == 0xCF8 && bridge.mechanism == 1)
        bridge.address = v;
    else if (port == 0xCF8)
        outb(0xCF8, (uint8_t)v), outb(0xCFA, (uint8_t)(v >> 16));
    else if (r)
        *r = v;
}

static const PciPorts FAKE = {inb, outb, ind, outd};

/* The card found by the bridge's mechanism, its BAR0 written and read back. */
static bool searched(const uint8_t mechanism)
{
    bridge = (typeof(bridge)){.mechanism = mechanism, .bar = 0xFB000000};

    PciFunction cards[4];
    const PciBus b = pci_bus(&FAKE);

    if (b.mechanism != mechanism || pci_cards(&b, cards, 4) != 1)
        return false;

    pci_write(&b, &cards[0], BAR0, 0xFA000000);

    return cards[0].device == CARD && cards[0].vendor_id == 0x121A && cards[0].device_id == 1 &&
           cards[0].class_code == VIDEO >> 8 && pci_read(&b, &cards[0], BAR0) == 0xFA000000;
}

TEST(pci_finds_a_card_by_mechanism_1) { CHECK(searched(1)); }

/* A 486 on Intel's 420ZX (86Box's 486sp3g) answers by mechanism #2 alone: by #1 alone, its Voodoo
   was not found and MEADOW16 drew in pages. */
TEST(pci_finds_a_card_by_mechanism_2)
{
    CHECK(searched(2));
    CHECK(!(bridge.enable & 0xF0)); /* the space closed after */
}

TEST(pci_finds_no_bus_where_neither_mechanism_answers)
{
    bridge = (typeof(bridge)){};

    PciFunction cards[4];
    const PciBus b = pci_bus(&FAKE);

    CHECK(!b.mechanism && !pci_cards(&b, cards, 4));
}
