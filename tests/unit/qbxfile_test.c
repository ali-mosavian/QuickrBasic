/* io/qbxfile.c: a .qbx's interfaces by name and this build's slice found, and a part outside the
   file refused rather than read past its end. */
#include "io/qbxfile.h"
#include "unit.h"
#include <string.h>

/* A .qbx as tools/qbx.py pack lays it out: one slice tagged tag, its bytes at 80 claiming bytes of
   them; one part named part, "IFACE" at 96 and 1Ah. */
static uint32_t make(
    uint8_t *const f,
    const char *const tag,
    const uint32_t bytes,
    const char *const part
)
{
    memset(f, 0, 112);
    memcpy(f, "QBX\x1a", 4);
    store16(f + 4, QBXF_FORMAT);
    store16(f + 6, 1);
    store16(f + 8, 0);
    store16(f + 10, 1);
    store16(f + 12, 1);
    strcpy((char *)f + 16, tag);
    store32(f + 32, 80);
    store32(f + 36, bytes);
    strcpy((char *)f + 40, part);
    store32(f + 56, 96);
    store32(f + 60, 5);
    memcpy(f + 80, "SLICE", 5);
    memcpy(f + 96, "IFACE\x1a", 6);
    return 112;
}

TEST(qbxfile_finds_interface_and_slice)
{
    (void)S;
    uint8_t f[112];
    QbxFile q;
    uint32_t n;
    CHECK(qbxf_read(f, make(f, "macos", 5, ""), "macos", &q));
    CHECK(q.abi == 1 && q.minor == 0);
    CHECK(q.nslice == 5 && q.slice && !memcmp(q.slice, "SLICE", 5));
    CHECK(qbxf_part(f, 112, "", &n) == f + 96 && n == 5);

    CHECK(qbxf_read(f, make(f, "macos", 5, ""), "wasm32", &q)); /* another platform's alone */
    CHECK(!q.slice);
    CHECK(qbxf_read(f, make(f, "mac", 5, ""), "macos", &q)); /* a tag's prefix is not it */
    CHECK(!q.slice);
    /* a package's part, by its name in any case: not its own interface */
    make(f, "macos", 5, "draw");
    CHECK(qbxf_part(f, 112, "DRAW", &n) == f + 96 && !qbxf_part(f, 112, "", &n));
    CHECK(!qbxf_part(f, 112, "dra", &n) && !qbxf_part(f, 112, "drawn", &n));
}

/* Version 1, before parts: the interface's offset and bytes in the head, the interface last. */
TEST(qbxfile_reads_version_1)
{
    (void)S;
    uint8_t f[96] = "QBX\x1a";
    store16(f + 4, 1);
    store16(f + 6, 1);
    store16(f + 10, 1);
    store32(f + 12, 80);
    store32(f + 16, 5);
    strcpy((char *)f + 20, "macos");
    store32(f + 36, 64);
    store32(f + 40, 5);
    memcpy(f + 64, "SLICE", 5);
    memcpy(f + 80, "IFACE", 5);
    QbxFile q;
    uint32_t n;
    CHECK(qbxf_read(f, 96, "macos", &q));
    CHECK(qbxf_part(f, 96, "", &n) == f + 80 && n == 5 && !qbxf_part(f, 96, "draw", &n));
    CHECK(q.nslice == 5 && !memcmp(q.slice, "SLICE", 5));
}

TEST(qbxfile_refuses_parts_outside_it)
{
    (void)S;
    uint8_t f[112];
    QbxFile q;
    uint32_t n;
    CHECK(qbxf_read(f, make(f, "macos", 33, ""), "macos", &q)); /* past the end: no slice */
    CHECK(!q.slice && qbxf_part(f, 112, "", &n));

    make(f, "macos", 5, "");
    store32(f + 60, 0xFFFFFFF0u); /* the interface past the end, wrapping */
    CHECK(qbxf_read(f, 112, "macos", &q) && !qbxf_part(f, 112, "", &n));

    make(f, "macos", 5, "");
    store16(f + 10, 4); /* more slices than the file holds */
    CHECK(!qbxf_read(f, 112, "macos", &q));

    make(f, "macos", 5, "");
    f[3] = 0;
    CHECK(!qbxf_read(f, 112, "macos", &q));
    CHECK(!qbxf_read(f, 15, "macos", &q));
}
