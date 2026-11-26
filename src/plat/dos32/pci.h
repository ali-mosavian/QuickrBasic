/* The PCI bus's configuration space, by mechanism #1 (ports CF8h and CFCh), as every PCI BIOS of a
 * 486 or later gives it: a function's registers read and written, and the bus searched for cards.
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

/* Register reg's double word (reg a multiple of 4). */
uint32_t pci_read(const PciFunction *f, uint8_t reg);
void pci_write(const PciFunction *f, uint8_t reg, uint32_t v);
/* The display and multimedia functions on every bus, at most most to out: how many; 0, no bus. */
uint32_t pci_cards(PciFunction *out, uint32_t most);
#endif
