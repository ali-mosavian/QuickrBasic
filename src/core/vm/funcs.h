/* The functions' interface. How it works: funcs.c. */
#ifndef CORE_VM_FUNC_H
#define CORE_VM_FUNC_H
#include "vm/exec.h"

/* Where t is in s from from, 1 the first; 0 none. INSTR's. */
int32_t func_find(Desc s, Desc t, int32_t from);
#endif
