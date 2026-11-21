/* util/image.c: a DOS slice's image read and placed: its words relocated by where it lies, the rest
   zero, and an image whose parts or relocations fall outside it refused. */
#include "util/image.h"
#include "unit.h"
#include <string.h>

/* An image as tools/qbx.py pack writes it: 12 bytes stored of 32, entry at 4, its words at 0 and 8
   relocated; relocs' last one at reloc. */
static uint32_t make(uint8_t *const s, const uint32_t reloc)
{
    memcpy(s, "QXI\x01", 4);
    store32(s + 4, 12);
    store32(s + 8, 32);
    store32(s + 12, 4);
    store32(s + 16, 2);
    store32(s + 20, 0x10);
    store32(s + 24, 0xC3C3C3C3);
    store32(s + 28, 0x20);
    store32(s + 32, 0);
    store32(s + 36, reloc);
    return 40;
}

TEST(image_placed_relocated_by_its_address)
{
    (void)S;
    uint8_t s[40], block[32];
    Image im;
    memset(block, 0xEE, sizeof block);
    CHECK(imag_read(s, make(s, 8), &im));
    CHECK(im.size == 32 && im.entry == 4);
    imag_place(&im, block, 0x40000);
    CHECK(load32(block) == 0x40010 && load32(block + 8) == 0x40020);
    CHECK(load32(block + 4) == 0xC3C3C3C3);                    /* not a relocation's: as stored */
    CHECK(load32(block + 12) == 0 && load32(block + 28) == 0); /* past what is stored: zero */
}

TEST(image_refuses_parts_outside_it)
{
    (void)S;
    uint8_t s[40];
    Image im;
    CHECK(!imag_read(s, make(s, 29), &im));    /* a word past its end */
    CHECK(!imag_read(s, make(s, 8) - 1, &im)); /* its relocations cut short */
    make(s, 8);
    store32(s + 8, 8); /* smaller than what is stored */
    CHECK(!imag_read(s, 40, &im));
    make(s, 8);
    store32(s + 12, 32); /* its entry outside */
    CHECK(!imag_read(s, 40, &im));
    make(s, 8);
    s[3] = 2; /* another version */
    CHECK(!imag_read(s, 40, &im));
}
