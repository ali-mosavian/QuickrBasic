/* base.h's byte helpers. */
#include "session.h"
#include "unit.h"

/* count_byte four at a time against one at a time: every length and alignment, the byte beside
   bytes whose low bits carry (7Fh, 80h, FFh, 01h). */
TEST(count_byte_counts_as_byte_by_byte)
{
    static const uint8_t mix[] = {0x00, 0x01, 0x7F, 0x80, 0x81, 0xFE, 0xFF, 0x80, 0x80, 0x01,
                                  0x80, 0x00, 0x7F, 0x80, 0x80, 0xFF, 0x80, 0x81, 0x80, 0x80};

    for (uint32_t b = 0; b < 256; b++)
        for (uint32_t from = 0; from < 4; from++)
            for (uint32_t n = 0; from + n <= sizeof mix; n++) {
                uint32_t want = 0;

                for (uint32_t i = 0; i < n; i++)
                    want += mix[from + i] == b;

                CHECK(count_byte(mix + from, n, (uint8_t)b) == want);
            }
}
