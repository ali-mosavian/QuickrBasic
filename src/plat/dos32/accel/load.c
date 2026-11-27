/* The accelerator's driver (accel.h): the bus searched once, then the files in lib/drivers/ read
 * until one names a card found (file.c). Only that driver is placed and kept.
 */
#include "plat/dos32/accel/accel.h"
#include "base.h"
#include "io/path.h"
#include "plat/dos32/dos.h"
#include "plat/pclk.h"
#include "plat/pfil.h"
#include "plat/pmem.h"
#include "plat/psys.h"
#include <string.h>

enum { CARDS = 8 };

static const char FOLDER[] = "lib/drivers/";
static const char PATTERN[] = "*.DRV";

static uint32_t ACCEL_CALL host_pci_read(const PciFunction *const card, const uint8_t reg)
{
    return pci_read(card, reg);
}

static void ACCEL_CALL
host_pci_write(const PciFunction *const card, const uint8_t reg, const uint32_t v)
{
    pci_write(card, reg, v);
}

static volatile uint8_t *ACCEL_CALL host_map(const uint32_t phys, const uint32_t bytes)
{
    return pdos_map(phys, bytes);
}

static uint8_t ACCEL_CALL host_inb(const uint16_t port) { return psys_inp(port); }

static void ACCEL_CALL host_outb(const uint16_t port, const uint8_t v) { psys_outp(port, v); }

static uint32_t ACCEL_CALL host_ind(const uint16_t port) { return pdos_ind(port); }

static void ACCEL_CALL host_outd(const uint16_t port, const uint32_t v) { pdos_outd(port, v); }

static void *ACCEL_CALL host_alloc(const uint32_t bytes)
{
    void *const p = pmem_resize(nullptr, bytes);

    if (p)
        memset(p, 0, bytes);

    return p;
}

static void ACCEL_CALL host_free(void *const p) { pmem_free(p); }

static uint64_t ACCEL_CALL host_clock_us(void) { return pclk_us(); }

static bool ACCEL_CALL
host_vesa(const uint16_t w, const uint16_t h, const uint8_t bits, AccelMode *const out)
{
    uint32_t n;
    PdosVbe card;
    const VbeMode *const modes = pdos_vbe_list(&n, &card);
    const int32_t i = vbe_holding(modes, n, w, h, bits);

    if (i < 0 || !pdos_vbe_set(&modes[i]))
        return false;

    *out = (AccelMode){modes[i].w, modes[i].h, modes[i].pitch, modes[i].base};
    return true;
}

static uint32_t ACCEL_CALL host_env(const char *const name, char *const out, const uint32_t cap)
{
    return pdos_env(name, out, cap);
}

static const AccelHost host = {
    .size = sizeof(AccelHost),
    .abi = ACCEL_ABI,
    .pci_read = host_pci_read,
    .pci_write = host_pci_write,
    .map = host_map,
    .inb = host_inb,
    .outb = host_outb,
    .ind = host_ind,
    .outd = host_outd,
    .alloc = host_alloc,
    .free = host_free,
    .clock_us = host_clock_us,
    .vesa = host_vesa,
    .env = host_env,
};

/* The search's: done once, and the driver and its card it found. */
static struct {
    bool searched;
    const AccelDriver *driver;
    PciFunction card;
} found;

/* Up to cap bytes of the file at path into buf: how many read; 0, none. */
static uint32_t read_up_to(const char *const path, uint8_t *const buf, const uint32_t cap)
{
    PfilHandle h;
    uint32_t got = 0;

    if (pfil_open(path, PFIL_READ, &h))
        return 0;

    if (pfil_read(h, buf, cap, &got))
        got = 0;

    pfil_close(h);
    return got;
}

/* The driver file's n bytes placed, its table asked for: null, it could not be. */
static const AccelDriver *placed(const uint8_t *const file, const uint32_t n)
{
    AccelInit *const init = (AccelInit *)pdos_place(file, n, "accel_init");
    const AccelDriver *const d = init ? init(&host) : nullptr;

    return d && d->size >= sizeof(AccelDriver) && d->abi == ACCEL_ABI ? d : nullptr;
}

/* The driver file at path, of bytes, placed if it drives one of cards' n, its card to *card: null,
   it drives none or could not be placed. */
static const AccelDriver *driving(
    const char *const path,
    const uint32_t bytes,
    const PciFunction *const cards,
    const uint32_t n,
    PciFunction *const card
)
{
    uint8_t *const file = pmem_resize(nullptr, bytes);
    const bool whole = file && read_up_to(path, file, bytes) == bytes;
    const AccelDriver *d = nullptr;

    for (uint32_t i = 0; whole && !d && i < n; i++) {
        d = accel_drives(file, bytes, &cards[i]) ? placed(file, bytes) : nullptr;

        if (d)
            *card = cards[i];
    }

    pmem_free(file);
    return d;
}

/* path, cap bytes, the folder of driver files under the interpreter's: its length; 0, too long. */
static uint32_t folder(char *const path, const uint32_t cap)
{
    if (!psys_self(path, cap))
        return 0;

    const uint32_t n = path_dir(path);

    if (n + sizeof FOLDER + sizeof PATTERN > cap)
        return 0;

    memcpy(path + n, FOLDER, sizeof FOLDER);
    return n + sizeof FOLDER - 1;
}

/* The first driver file in lib/drivers/ that drives one of cards' n, placed, its card to *card. */
static const AccelDriver *search(
    const PciFunction *const cards,
    const uint32_t n,
    PciFunction *const card
)
{
    char path[PFIL_PATH];
    PfilFind f;
    const uint32_t at = folder(path, sizeof path);

    if (!at)
        return nullptr;

    memcpy(path + at, PATTERN, sizeof PATTERN);

    for (uint16_t e = pfil_first(path, 0, &f); !e; e = pfil_next(&f)) {
        const uint32_t name = (uint32_t)strlen(f.name);

        if (at + name >= sizeof path)
            continue;

        memcpy(path + at, f.name, name + 1);
        const AccelDriver *const d = driving(path, f.size, cards, n, card);

        if (d)
            return d;
    }

    return nullptr;
}

const AccelDriver *accel_driver(PciFunction *const card)
{
    if (!found.searched) {
        PciFunction cards[CARDS];
        const uint32_t n = pci_cards(cards, CARDS);

        found.searched = true;
        found.driver = n ? search(cards, n, &found.card) : nullptr;
    }

    *card = found.card;
    return found.driver;
}
