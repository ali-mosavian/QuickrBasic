/* The PCI bus's configuration space: a function's registers read and written, and the bus searched
 * for cards. By mechanism #1 (an address at CF8h, data at CFCh), as most chipsets give it, else by
 * mechanism #2 (a function at CF8h, the bus at CFAh, a device's registers at C000h up), as 486
 * boards on Intel's 420TX and 420ZX do. Found as Linux finds them.
 */
#ifndef PLAT_DOS32_PCI_H
#define PLAT_DOS32_PCI_H
#include <stdbool.h>
#include <stdint.h>

/* A function: its place, its vendor's and device's IDs, and its class (base, sub and interface). */
typedef struct {
    uint8_t bus, device, function;
    uint16_t vendor_id, device_id;
    uint32_t class_code;
} PciFunction;

/* PCI's display and multimedia classes, a class code's top byte. */
enum : uint8_t { PCI_DISPLAY = 0x03, PCI_MULTIMEDIA = 0x04 };

/* The I/O ports the bus is reached through: a byte's and a double word's. */
typedef struct {
    uint8_t (*inb)(uint16_t port);
    void (*outb)(uint16_t port, uint8_t v);
    uint32_t (*ind)(uint16_t port);
    void (*outd)(uint16_t port, uint32_t v);
} PciPorts;

/* The bus: its ports and its mechanism, 1 or 2; 0, neither answers. */
typedef struct {
    const PciPorts *ports;
    uint8_t mechanism;
} PciBus;

/* The bus through ports, by mechanism #1 if it answers, else #2. */
PciBus pci_bus(const PciPorts *ports);
/* Register reg's double word (reg a multiple of 4). */
uint32_t pci_read(const PciBus *b, const PciFunction *f, uint8_t reg);
void pci_write(const PciBus *b, const PciFunction *f, uint8_t reg, uint32_t v);
/* The display and multimedia functions on every bus, at most most to out: how many; 0, no bus. */
uint32_t pci_cards(const PciBus *b, PciFunction *out, uint32_t most);
#endif
