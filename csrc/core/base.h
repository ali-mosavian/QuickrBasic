/* What every core module includes: the fixed-width types, the arena and its addresses, a data
   stack cell. Portable C23; the platform is behind plat/. */
#ifndef CORE_BASE_H
#define CORE_BASE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Memory a BASIC program can see (variables, strings, arrays, records, the POKE memory) and every
   structure that keeps an address of it lives in one arena. An Addr is an offset in it, 32 bits on
   every host, so records keep the asm build's layout. On DOS32 the arena's origin is the flat
   segment's 0: an Addr is the address, as VARPTR and PEEK see it. */
typedef uint32_t Addr;

extern uint8_t *mem_base; /* Addr 0; set once by pmem_init (plat/pmem.h) */

static inline uint8_t *at(Addr a) { return mem_base + a; }
static inline Addr addr_of(const void *p) { return (Addr)((const uint8_t *)p - mem_base); }

static inline uint8_t rd8(Addr a) { return *at(a); }
static inline uint16_t rd16(Addr a) { uint16_t v; memcpy(&v, at(a), 2); return v; }
static inline uint32_t rd32(Addr a) { uint32_t v; memcpy(&v, at(a), 4); return v; }
static inline void wr8(Addr a, uint8_t v) { *at(a) = v; }
static inline void wr16(Addr a, uint16_t v) { memcpy(at(a), &v, 2); }
static inline void wr32(Addr a, uint32_t v) { memcpy(at(a), &v, 4); }

/* A data stack cell: an INT, a FLOAT, or an Addr (a string's Desc) in its low dword; a DOUBLE
   fills it. */
typedef union {
    int32_t i;
    uint32_t u;
    float f;
    Addr a;
    double d;
    uint64_t raw;
} Cell;
static_assert(sizeof(Cell) == 8);

/* A record laid out byte for byte as the asm's struc: no padding. */
#define PACKED [[gnu::packed]]

#define countof(a) (sizeof(a) / sizeof((a)[0]))
#endif
