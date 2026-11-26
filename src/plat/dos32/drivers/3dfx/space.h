/* A card's memory given out in ranges: texture memory's bytes, or a frame buffer's rows. */
#ifndef TDFX_SPACE_H
#define TDFX_SPACE_H
#include <stdbool.h>
#include <stdint.h>

enum : uint32_t { TDFX_RANGES = 1024, TDFX_NONE = UINT32_MAX };

typedef struct {
    uint32_t at, n;
} TdfxRange;

/* size units, used kept in order of place. */
typedef struct {
    TdfxRange used[TDFX_RANGES];
    uint32_t count, size;
} TdfxSpace;

/* n units at a multiple of align (a power of two), the first room for them: their place, TDFX_NONE
   none. */
uint32_t tdfx_take(TdfxSpace *s, uint32_t n, uint32_t align);
/* The range at at given back. */
void tdfx_give(TdfxSpace *s, uint32_t at);
#endif
