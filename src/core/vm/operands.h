/* The simple ops' interface. How it works: operands.c. */
#ifndef CORE_VM_OPER_H
#define CORE_VM_OPER_H
#include "vm/exec.h"

/* A compare op's operand: the relations it is true for. */
enum : uint8_t { OPER_GT = 1, OPER_EQ = 2, OPER_LT = 4 };
#endif
