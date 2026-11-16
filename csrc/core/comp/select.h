/* Each op's addressing mode. How it works: select.c. */
#ifndef COMP_SELECT_H
#define COMP_SELECT_H
#include "comp/blocks.h"
#include "mem/symbols.h"

/* The op at op, its operands as compiled (ids), its frames laid out: its mode and its operands'
   places. */
void select_op(const Symb *sy, const Blok *b, uint8_t *op);
#endif
