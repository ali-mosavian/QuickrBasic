/* A DLL made for the tests of its loader (util/image.c) and of driver files. How it works: dll.c. */
#ifndef DLL_H
#define DLL_H
#include "base.h"

/* Its file's bytes; where it is based; its one section's offset in memory and its size there; the
   image's size; the section's bytes stored; DLL_DATA, the offset of bytes it leaves to a test. */
enum : uint32_t {
    DLL_BYTES = 0x400,
    DLL_BASE = 0x10000000,
    DLL_SECTION = 0x1000,
    DLL_SIZE = 0x3000,
    DLL_STORED = 0x200,
    DLL_DATA = 0x1040,
    DLL_EXPORTS = 4,
};

/* A DLL in file, DLL_BYTES, its exports names[n] at offsets at[n]: its file's bytes. */
uint32_t dll_make(uint8_t *file, const char *const *names, const uint32_t *at, uint32_t n);
/* The byte of file stored for offset at in memory. */
uint8_t *dll_at(uint8_t *file, uint32_t at);
#endif
