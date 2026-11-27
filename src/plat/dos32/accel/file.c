/* A driver file (driver.h): the PCI IDs its DLL exports. It calls nothing of DOS's, so unit tests
 * run it.
 */
#include "base.h"
#include "plat/dos32/accel/accel.h"
#include "util/image.h"

bool accel_drives(const uint8_t *const file, const uint32_t n, const PciFunction *const f)
{
    const uint32_t want = ACCEL_ID(f->vendor_id, f->device_id);
    Image im;
    uint32_t ids;

    if (!imag_read(file, n, &im) || !imag_export(&im, "accel_ids", &ids))
        return false;

    for (uint32_t i = 0;; i++) {
        const uint8_t *const id = imag_bytes(&im, ids + 4 * i, 4);

        if (!id || !load32(id))
            return false;

        if (load32(id) == want)
            return true;
    }
}
