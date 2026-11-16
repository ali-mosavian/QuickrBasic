/* What every core module includes: the fixed-width types, the arena and its addresses, a data
 * stack cell.
 *
 * # How it works
 * Portable C23; the platform is behind plat/. Memory a BASIC program can see (variables, strings,
 * arrays, records, the POKE memory) and every structure that keeps an address of it lives in one
 * arena. An Addr is an offset in it, 32 bits on every host, so records keep the asm build's layout.
 * On DOS32 the arena's origin is the flat segment's 0: an Addr is the address, as VARPTR and PEEK
 * see it.
 */
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

/* The type whose member p points to. */
#define container_of(p, type, member) ((type *)((uint8_t *)(p) - offsetof(type, member)))

/* A BASIC session's state (session.h). */
typedef struct Session Session;

#if ARENA_AT_ZERO /* the platform's arena starts at address 0 (DOS32's flat segment) */
static inline uint8_t *at(const Addr a) { return (uint8_t *)(uintptr_t)a; }
static inline Addr addr_of(const void *const p) { return (Addr)(uintptr_t)p; }
#else
extern uint8_t *mem_base; /* Addr 0; set once by pmem_init (plat/pmem.h) */

static inline uint8_t *at(const Addr a) { return mem_base + a; }
static inline Addr addr_of(const void *const p) { return (Addr)((const uint8_t *)p - mem_base); }
#endif

/* Words at any address: the 386 loads them unaligned, and may_alias lets them read any object's
   bytes. */
typedef uint16_t __attribute__((aligned(1), may_alias)) Word16;
typedef uint32_t __attribute__((aligned(1), may_alias)) Word32;

static inline uint16_t load16(const void *const p) { return *(const Word16 *)p; }
static inline uint32_t load32(const void *const p) { return *(const Word32 *)p; }
static inline void store16(void *const p, const uint16_t v) { *(Word16 *)p = v; }
static inline void store32(void *const p, const uint32_t v) { *(Word32 *)p = v; }

static inline uint8_t rd8(const Addr a) { return *at(a); }
static inline uint16_t rd16(const Addr a) { return load16(at(a)); }
static inline uint32_t rd32(const Addr a) { return load32(at(a)); }
static inline void wr8(const Addr a, const uint8_t v) { *at(a) = v; }
static inline void wr16(const Addr a, const uint16_t v) { store16(at(a), v); }
static inline void wr32(const Addr a, const uint32_t v) { store32(at(a), v); }

static inline uint8_t upcase(const uint8_t c) { return c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c; }
static inline bool is_letter(const uint8_t c) { return (uint8_t)(upcase(c) - 'A') < 26; }
static inline bool is_digit(const uint8_t c) { return (uint8_t)(c - '0') < 10; }
static inline bool is_blank(const uint8_t c) { return c == ' ' || c == '\t'; }

/* The bytes b in [p, p+n), four at a time: byte by byte, symb_size spent 3% of a link. */
static inline uint32_t count_byte(const uint8_t *p, uint32_t n, const uint8_t b)
{
    uint32_t c = 0;

    for (; n >= 4; p += 4, n -= 4) {
        const uint32_t x = load32(p) ^ b * 0x01010101u; /* a 0 byte where b was */
        const uint32_t z = ~((x & 0x7F7F7F7Fu) + 0x7F7F7F7Fu | x | 0x7F7F7F7Fu); /* its bit 7 */
        c += (z >> 7) * 0x01010101u >> 24;
    }

    for (; n; n--)
        c += *p++ == b;

    return c;
}

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

typedef Cell __attribute__((aligned(1), may_alias)) AnyCell;

/* The 8 bytes at p as a Cell, and back. */
static inline Cell load_cell(const void *const p) { return *(const AnyCell *)p; }
static inline void store_cell(void *const p, const Cell c) { *(AnyCell *)p = c; }

/* A record laid out byte for byte as the asm's struc: no padding. */
#define PACKED [[gnu::packed]]

#define countof(a) (sizeof(a) / sizeof((a)[0]))
#endif
