/* io/qbxfile.c: a .qbx's interface and this build's slice found, and a part outside the file
   refused rather than read past its end. */
#include "io/qbxfile.h"
#include "unit.h"
#include <string.h>

/* A .qbx as tools/qbx.py pack lays it out: the interface "IFACE" at 64, one slice tagged tag, its
   bytes at 80 claiming bytes of them. */
static uint32_t make(uint8_t *const f, const char *const tag, const uint32_t bytes)
{
    memset(f, 0, 96);
    memcpy(f, "QBX\x1a", 4);
    store16(f + 4, QBXF_FORMAT);
    store16(f + 6, 1);
    store16(f + 8, 0);
    store16(f + 10, 1);
    store32(f + 12, 64);
    store32(f + 16, 5);
    strcpy((char *)f + 20, tag);
    store32(f + 36, 80);
    store32(f + 40, bytes);
    memcpy(f + 64, "IFACE", 5);
    memcpy(f + 80, "SLICE", 5);
    return 96;
}

TEST(qbxfile_finds_interface_and_slice)
{
    (void)S;
    uint8_t f[96];
    QbxFile q;
    CHECK(qbxf_read(f, make(f, "macos", 5), "macos", &q));
    CHECK(q.abi == 1 && q.minor == 0);
    CHECK(q.ninterface == 5 && !memcmp(q.interface, "IFACE", 5));
    CHECK(q.nslice == 5 && q.slice && !memcmp(q.slice, "SLICE", 5));

    CHECK(qbxf_read(f, make(f, "macos", 5), "wasm32", &q)); /* another platform's alone */
    CHECK(!q.slice);
    CHECK(qbxf_read(f, make(f, "mac", 5), "macos", &q)); /* a tag's prefix is not it */
    CHECK(!q.slice);
}

TEST(qbxfile_refuses_parts_outside_it)
{
    (void)S;
    uint8_t f[96];
    QbxFile q;
    CHECK(qbxf_read(f, make(f, "macos", 17), "macos", &q)); /* past the end: no slice */
    CHECK(!q.slice);

    make(f, "macos", 5);
    store32(f + 16, 0xFFFFFFF0u); /* the interface past the end, wrapping */
    CHECK(!qbxf_read(f, 96, "macos", &q));

    make(f, "macos", 5);
    store16(f + 10, 4); /* more slices than the file holds */
    CHECK(!qbxf_read(f, 96, "macos", &q));

    make(f, "macos", 5);
    f[3] = 0;
    CHECK(!qbxf_read(f, 96, "macos", &q));
    CHECK(!qbxf_read(f, 19, "macos", &q));
}
