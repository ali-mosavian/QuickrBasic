/* A .qbx's parts: its interfaces, by name, and this build's slice.
 *
 * # How it works
 * tools/qbx.py pack writes it, little-endian: "QBX" 1Ah; the format's version, the ABI's major and
 * the minor it needs, the slices' and the parts' counts (16 bits each), 2 bytes 0; then each
 * slice's tag (16 bytes, 0-padded), offset and bytes; then each part's name the same way, "" the
 * module's own interface. The slices follow, then the parts, each followed by 1Ah: the loader reads
 * one as a source file from its offset (io/repl.c), and a source file ends at 1Ah. Version 1 had no
 * parts: the interface's offset and bytes followed the slices' count, the interface last. The
 * reader checks every part lies inside the file: a .qbx is read from wherever the program is.
 */
#include "io/qbxfile.h"
#include <string.h>

enum { HEAD1 = 20, HEAD = 16, ENTRY = 24, NAME = 16 };

/* The n bytes at offset at lie inside the file's size. */
static bool inside(const uint32_t size, const uint32_t at, const uint32_t n)
{
    return at <= size && n <= size - at;
}

bool qbxf_is(const char *const path)
{
    const size_t n = strlen(path);
    return n > 4 && path[n - 4] == '.' && upcase((uint8_t)path[n - 3]) == 'Q' &&
           upcase((uint8_t)path[n - 2]) == 'B' && upcase((uint8_t)path[n - 1]) == 'X';
}

/* The entry named name, without case, among the table's n at offset table: its bytes, *bytes of
   them; null none, or one outside the file. */
static const uint8_t *entry(
    const uint8_t *const data,
    const uint32_t size,
    const uint32_t table,
    const uint32_t n,
    const char *const name,
    uint32_t *const bytes
)
{
    const uint32_t len = (uint32_t)strlen(name);

    for (uint32_t i = 0; i < n && len < NAME; i++) {
        const uint8_t *const e = data + table + i * ENTRY;
        const uint32_t at = load32(e + NAME), got = load32(e + NAME + 4);
        uint32_t k = 0;

        while (k < len && upcase(e[k]) == upcase((uint8_t)name[k]))
            k++;

        if (k == len && !e[len] && inside(size, at, got)) {
            *bytes = got;
            return data + at;
        }
    }

    return nullptr;
}

/* data's tables, a .qbx's: the slices' at *slices, n of them; the parts' at *parts, n of them.
   false: not one, or a table outside it. */
static bool tables(
    const uint8_t *const data,
    const uint32_t size,
    uint32_t *const slices,
    uint32_t *const nslices,
    uint32_t *const parts,
    uint32_t *const nparts
)
{
    if (size < HEAD || memcmp(data, "QBX\x1a", 4))
        return false;

    const uint16_t format = load16(data + 4);
    *slices = format == 1 ? HEAD1 : HEAD;
    *nslices = load16(data + 10);
    *nparts = format == 1 ? 0 : load16(data + 12);
    *parts = *slices + *nslices * ENTRY;
    return (format == 1 || format == QBXF_FORMAT) &&
           inside(size, *slices, (*nslices + *nparts) * ENTRY);
}

bool qbxf_read(
    const uint8_t *const data,
    const uint32_t size,
    const char *const tag,
    QbxFile *const out
)
{
    uint32_t slices, nslices, parts, nparts;

    if (!tables(data, size, &slices, &nslices, &parts, &nparts))
        return false;

    *out = (QbxFile){.abi = load16(data + 6), .minor = load16(data + 8)};

    if (*tag)
        out->slice = entry(data, size, slices, nslices, tag, &out->nslice);

    return true;
}

const uint8_t *qbxf_part(
    const uint8_t *const data,
    const uint32_t size,
    const char *const name,
    uint32_t *const bytes
)
{
    uint32_t slices, nslices, parts, nparts;

    if (!tables(data, size, &slices, &nslices, &parts, &nparts))
        return nullptr;

    if (load16(data + 4) != 1)
        return entry(data, size, parts, nparts, name, bytes);

    const uint32_t at = load32(data + 12), n = load32(data + 16); /* version 1: its own alone */

    if (*name || !inside(size, at, n))
        return nullptr;

    *bytes = n;
    return data + at;
}
