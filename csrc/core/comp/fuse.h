/* Op fusion's interface. How it works: fuse.c. */
#ifndef COMP_FUSE_H
#define COMP_FUSE_H
#include "defs.h"

/* The code from start to end with each fusable sequence one op; returns its new end. */
uint8_t *fuse_code(uint8_t *start, uint8_t *end);
#endif
