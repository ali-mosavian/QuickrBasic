/* A .qbx's parts: its own interface and this build's slice.
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

/* The entry named name among the table's n, at offset table: its bytes, *bytes of them; null none,
   or one outside the file. */
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

        if (!memcmp(e, name, len + 1) && inside(size, at, got)) {
            *bytes = got;
            return data + at;
        }
    }

    return nullptr;
}

bool qbxf_read(
    const uint8_t *const data,
    const uint32_t size,
    const char *const tag,
    QbxFile *const out
)
{
    if (size < HEAD || memcmp(data, "QBX\x1a", 4))
        return false;

    const uint16_t format = load16(data + 4);
    const uint32_t nslices = load16(data + 10);
    const uint32_t table = format == 1 ? HEAD1 : HEAD;
    const uint32_t nparts = format == 1 ? 0 : load16(data + 12);

    if ((format != 1 && format != QBXF_FORMAT) || !inside(size, table, (nslices + nparts) * ENTRY))
        return false;

    *out = (QbxFile){.abi = load16(data + 6), .minor = load16(data + 8)};

    if (format == 1) {
        const uint32_t at = load32(data + 12), n = load32(data + 16);

        if (!inside(size, at, n))
            return false;

        out->interface = data + at;
        out->ninterface = n;
    } else {
        out->interface = entry(data, size, table + nslices * ENTRY, nparts, "", &out->ninterface);
    }

    if (*tag)
        out->slice = entry(data, size, table, nslices, tag, &out->nslice);

    return true;
}
