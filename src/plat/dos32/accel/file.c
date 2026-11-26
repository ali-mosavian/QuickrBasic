/* A driver file (driver.h): its header's IDs and its image. It calls nothing of DOS's, so unit
 * tests run it.
 */
#include "base.h"
#include "plat/dos32/accel/accel.h"
#include <string.h>

/* The header's IDs' count, of a file of n bytes: false, not a driver file of this ABI. */
static bool ids_of(const uint8_t *const file, const uint32_t n, uint32_t *const ids)
{
    if (n < ACCEL_HEAD || memcmp(file, "QXD\1", 4) || load32(file + 4) != ACCEL_ABI)
        return false;

    *ids = load32(file + 8);
    return true;
}

bool accel_drives(const uint8_t *const head, const uint32_t n, const PciFunction *const f)
{
    const uint32_t want = (uint32_t)f->vendor_id << 16 | f->device_id;
    uint32_t ids;

    if (!ids_of(head, n, &ids))
        return false;

    for (uint32_t i = 0; i < ids && i < (n - ACCEL_HEAD) / 4; i++)
        if (load32(head + ACCEL_HEAD + 4 * i) == want)
            return true;

    return false;
}

bool accel_image(
    const uint8_t *const file,
    const uint32_t n,
    const uint8_t **const image,
    uint32_t *const bytes
)
{
    uint32_t ids;

    if (!ids_of(file, n, &ids) || ids >= (n - ACCEL_HEAD) / 4)
        return false;

    *image = file + ACCEL_HEAD + 4 * ids;
    *bytes = n - ACCEL_HEAD - 4 * ids;
    return true;
}
