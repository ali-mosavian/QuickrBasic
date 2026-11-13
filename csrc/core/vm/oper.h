/* The simple ops' handlers (src/vm/bc.asm): loads, stores, INT and FLOAT operators, compares,
   IF, conversions, DROP; reached only through the op table (vm/ops.def).

   A compare op's operand is its mask, the relations it is true for: OPER_GT, OPER_EQ, OPER_LT.
   A compare pushes -1 or 0; a CMPIF's false, like a false IF, ends the line (flow_nextln). */
#ifndef CORE_VM_OPER_H
#define CORE_VM_OPER_H
#include "vm/exec.h"

enum : uint8_t { OPER_GT = 1, OPER_EQ = 2, OPER_LT = 4 };

/* g_relbit (calls.asm): the masks of > = < by TK_GT - TK_GT .. TK_LT - TK_GT, for the compiler. */
extern const uint8_t oper_relbit[3];
#endif
