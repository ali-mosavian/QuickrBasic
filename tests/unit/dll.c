/* A DLL made for the tests: as lld-link lays one out, based at DLL_BASE, its one section at 1000h,
 * 200h bytes stored of 2000h in memory. Its words at 1000h and 1008h hold the addresses of 1010h
 * and 1020h, relocated; 1004h holds code. Its export directory is at 1100h, its relocations at
 * 1190h.
 */
#include "dll.h"
#include <string.h>

enum : uint32_t {
    PE = 0x40,
    OPTIONAL = PE + 24,
    TABLE = OPTIONAL + 224,
    RAW = 0x200,
    EXPORTS = 0x1100,
    FUNCS = 0x1128,
    NAMES = 0x1138,
    ORDS = 0x1148,
    STRINGS = 0x1150,
    NAME = 16,
    RELOCS = 0x1190,
    HIGHLOW = 3u << 12,
};

uint8_t *dll_at(uint8_t *const file, const uint32_t at) { return file + RAW + at - DLL_SECTION; }

uint32_t dll_make(
    uint8_t *const file,
    const char *const *const names,
    const uint32_t *const at,
    const uint32_t n
)
{
    memset(file, 0, DLL_BYTES);
    memcpy(file, "MZ", 2);
    store32(file + 0x3C, PE);
    memcpy(file + PE, "PE\0\0", 4);
    store16(file + PE + 4, 0x14C);
    store16(file + PE + 6, 1);
    store16(file + PE + 20, 224);
    store16(file + PE + 22, 0x2102);
    store16(file + OPTIONAL, 0x10B);
    store32(file + OPTIONAL + 28, DLL_BASE);
    store32(file + OPTIONAL + 56, DLL_SIZE);
    store32(file + OPTIONAL + 92, 16);
    store32(file + OPTIONAL + 96, EXPORTS);
    store32(file + OPTIONAL + 100, 40);
    store32(file + OPTIONAL + 96 + 40, RELOCS);
    store32(file + OPTIONAL + 96 + 44, 12);
    memcpy(file + TABLE, ".text", 5);
    store32(file + TABLE + 8, DLL_SIZE - DLL_SECTION);
    store32(file + TABLE + 12, DLL_SECTION);
    store32(file + TABLE + 16, DLL_STORED);
    store32(file + TABLE + 20, RAW);

    store32(dll_at(file, 0x1000), DLL_BASE + 0x1010);
    store32(dll_at(file, 0x1004), 0xC3C3C3C3);
    store32(dll_at(file, 0x1008), DLL_BASE + 0x1020);

    store32(dll_at(file, EXPORTS + 20), n);
    store32(dll_at(file, EXPORTS + 24), n);
    store32(dll_at(file, EXPORTS + 28), FUNCS);
    store32(dll_at(file, EXPORTS + 32), NAMES);
    store32(dll_at(file, EXPORTS + 36), ORDS);

    for (uint32_t i = 0; i < n && i < DLL_EXPORTS; i++) {
        store32(dll_at(file, FUNCS + 4 * i), at[i]);
        store32(dll_at(file, NAMES + 4 * i), STRINGS + NAME * i);
        store16(dll_at(file, ORDS + 2 * i), (uint16_t)i);
        memcpy(dll_at(file, STRINGS + NAME * i), names[i], strlen(names[i]) + 1);
    }

    store32(dll_at(file, RELOCS), 0x1000);
    store32(dll_at(file, RELOCS + 4), 12);
    store16(dll_at(file, RELOCS + 8), HIGHLOW | 0x000);
    store16(dll_at(file, RELOCS + 10), HIGHLOW | 0x008);
    return DLL_BYTES;
}
