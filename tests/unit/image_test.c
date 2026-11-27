/* util/image.c: a DOS slice's DLL read and placed: its words relocated by where it lies, the rest
   zero, its exports found by name, and a DLL whose parts or relocations fall outside it refused. */
#include "util/image.h"
#include "dll.h"
#include "unit.h"
#include <string.h>

static const char *const ENTRY[] = {"qx_init_t"};
static const uint32_t ENTRY_AT[] = {0x1004};

TEST(image_placed_relocated_by_its_address)
{
    (void)S;
    uint8_t file[DLL_BYTES], block[DLL_SIZE];
    Image im;
    uint32_t at;
    memset(block, 0xEE, sizeof block);
    CHECK(imag_read(file, dll_make(file, ENTRY, ENTRY_AT, 1), &im));
    CHECK(im.size == DLL_SIZE && imag_export(&im, "qx_init_t", &at) && at == 0x1004);
    imag_place(&im, block, 0x40000);
    CHECK(load32(block + 0x1000) == 0x41010 && load32(block + 0x1008) == 0x41020);
    CHECK(load32(block + 0x1004) == 0xC3C3C3C3); /* not a relocation's: as stored */
    CHECK(load32(block) == 0);                   /* the headers: not placed */
    CHECK(load32(block + DLL_SECTION + DLL_STORED) == 0 && load32(block + DLL_SIZE - 4) == 0);
}

/* An export is found by its whole name, not one it starts. */
TEST(image_exports_found_by_name)
{
    (void)S;
    uint8_t file[DLL_BYTES];
    Image im;
    uint32_t at;
    CHECK(imag_read(file, dll_make(file, ENTRY, ENTRY_AT, 1), &im));
    CHECK(!imag_export(&im, "qx_init_", &at) && !imag_export(&im, "qx_init_tt", &at));
    CHECK(imag_read(file, dll_make(file, ENTRY, ENTRY_AT, 0), &im));
    CHECK(!imag_export(&im, "qx_init_t", &at));
}

TEST(image_refuses_parts_outside_it)
{
    (void)S;
    uint8_t file[DLL_BYTES];
    Image im;
    const uint32_t n = dll_make(file, ENTRY, ENTRY_AT, 1);
    CHECK(!imag_read(file, n - 1, &im)); /* its section cut short */
    store32(dll_at(file, 0x1190), DLL_SIZE - DLL_SECTION);
    store16(dll_at(file, 0x1198), 3u << 12 | 0xFFE); /* a word past its end */
    CHECK(!imag_read(file, n, &im));
    dll_make(file, ENTRY, ENTRY_AT, 1);
    store16(dll_at(file, 0x1198), 10u << 12); /* a 64-bit word's */
    CHECK(!imag_read(file, n, &im));
    dll_make(file, ENTRY, ENTRY_AT, 1);
    store32(file + 0x58 + 96 + 12, 40); /* it imports */
    CHECK(!imag_read(file, n, &im));
    dll_make(file, ENTRY, ENTRY_AT, 1);
    store16(file + 0x44, 0x8664); /* not i386's */
    CHECK(!imag_read(file, n, &im));
}
