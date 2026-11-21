/* A .qbx's parts: its interface and this build's slice.
 *
 * # How it works
 * tools/qbx.py pack writes it, little-endian: "QBX" 1Ah; the format's version, the ABI's major and
 * the minor it needs, the slices' count (16 bits each); the interface's offset and bytes (32 bits
 * each); then each slice's tag (16 bytes, 0-padded), offset and bytes. The slices follow, then the
 * interface, last, so the loader reads it as a source file from its offset (io/repl.c). The reader
 * checks every part lies inside the file: a .qbx is read from wherever the program is.
 */
#include "io/qbxfile.h"
#include <string.h>

enum { HEAD = 20, SLICE = 24, TAG = 16 };

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

bool qbxf_read(
    const uint8_t *const data,
    const uint32_t size,
    const char *const tag,
    QbxFile *const out
)
{
    if (size < HEAD || memcmp(data, "QBX\x1a", 4) || load16(data + 4) != QBXF_FORMAT)
        return false;

    const uint32_t nslices = load16(data + 10), at = load32(data + 12), n = load32(data + 16);

    if (!inside(size, HEAD, nslices * SLICE) || !inside(size, at, n))
        return false;

    *out = (QbxFile){
        .abi = load16(data + 6),
        .minor = load16(data + 8),
        .interface = data + at,
        .ninterface = n,
    };

    const uint32_t len = (uint32_t)strlen(tag);

    for (uint32_t i = 0; i < nslices && len && len < TAG; i++) {
        const uint8_t *const s = data + HEAD + i * SLICE;
        const uint32_t where = load32(s + TAG), bytes = load32(s + TAG + 4);

        if (!memcmp(s, tag, len + 1) && inside(size, where, bytes)) {
            out->slice = data + where;
            out->nslice = bytes;
        }
    }

    return true;
}
