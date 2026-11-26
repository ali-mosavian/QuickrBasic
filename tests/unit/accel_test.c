/* plat/dos32/accel/file.c: a driver file's header read, as the loader reads one for each card. */
#include "plat/dos32/accel/accel.h"
#include "unit.h"
#include <string.h>

/* A driver file of Voodoo 1 and 2's IDs, its image's four bytes after them. */
static const uint8_t VOODOO[] = {
    'Q',  'X',  'D',  1,    ACCEL_ABI, 0,    0,    0,    2,   0,   0,   0,
    0x01, 0x00, 0x1A, 0x12, 0x02,      0x00, 0x1A, 0x12, 'Q', 'X', 'I', 1,
};

/* A header names the cards it drives by vendor and device: a Banshee's is not among them. */
TEST(accel_driver_files_name_their_cards)
{
    const PciFunction one = {.vendor_id = 0x121A, .device_id = 0x0001};

    const PciFunction two = {.vendor_id = 0x121A, .device_id = 0x0002};

    const PciFunction banshee = {.vendor_id = 0x121A, .device_id = 0x0003};

    CHECK(accel_drives(VOODOO, sizeof VOODOO, &one) && accel_drives(VOODOO, sizeof VOODOO, &two));
    CHECK(!accel_drives(VOODOO, sizeof VOODOO, &banshee));
}

/* A header read short names only the IDs it holds; another magic or ABI names none. */
TEST(accel_driver_files_are_read_whole_or_not_at_all)
{
    const PciFunction two = {.vendor_id = 0x121A, .device_id = 0x0002};

    uint8_t other[sizeof VOODOO];

    CHECK(!accel_drives(VOODOO, ACCEL_HEAD + 4, &two));
    memcpy(other, VOODOO, sizeof other);
    other[4] = ACCEL_ABI + 1;
    CHECK(!accel_drives(other, sizeof other, &two));
    other[4] = ACCEL_ABI;
    other[3] = 2;
    CHECK(!accel_drives(other, sizeof other, &two));
}

/* The image follows the IDs; a file whose IDs run past it has none. */
TEST(accel_driver_files_hold_their_image_after_the_ids)
{
    const uint8_t *image;
    uint32_t n;

    CHECK(accel_image(VOODOO, sizeof VOODOO, &image, &n) && n == 4 && !memcmp(image, "QXI", 3));
    CHECK(!accel_image(VOODOO, ACCEL_HEAD + 8, &image, &n));
}
